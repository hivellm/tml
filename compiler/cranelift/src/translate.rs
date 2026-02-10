/// MIR → Cranelift IR Translation
///
/// Translates deserialized MIR instructions into Cranelift IR.
/// Handles phi-to-block-parameter conversion, all Tier 1 instructions
/// (arithmetic, calls, casts, constants, alloca, load/store, terminators),
/// and Tier 2 aggregates (struct/enum/tuple/array init, GEP, extract/insert).

use std::collections::HashMap;

use cranelift_codegen::ir::{
    condcodes::{FloatCC, IntCC},
    types, AbiParam, Block, BlockArg, Function as ClifFunc, InstBuilder, MemFlags, StackSlotData,
    StackSlotKind, TrapCode, Value as ClifValue,
};
use cranelift_codegen::settings::{self, Configurable};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext};
use cranelift_module::{FuncId, Linkage, Module};
use cranelift_object::{ObjectBuilder, ObjectModule};

use crate::error::{BridgeError, BridgeResult};
use crate::mir_types::*;
use crate::types::{self as ty, POINTER_TYPE};

/// Translator state for a single module compilation.
pub struct ModuleTranslator {
    pub module: ObjectModule,
    /// Maps symbol name → Cranelift FuncId (keys use tml_ prefix for user funcs)
    func_ids: HashMap<String, FuncId>,
    /// Struct definitions from MIR module (for layout computation)
    struct_defs: HashMap<String, Vec<StructField>>,
    /// Enum definitions from MIR module
    enum_defs: HashMap<String, Vec<EnumVariant>>,
    /// Set of C runtime function names (these do NOT get tml_ prefix)
    runtime_names: std::collections::HashSet<String>,
}

impl ModuleTranslator {
    pub fn new(target_triple: &str, opt_level: u8) -> BridgeResult<Self> {
        let isa_builder = cranelift_native::builder().map_err(|e| {
            BridgeError::InvalidTarget(format!("failed to create native ISA builder: {}", e))
        })?;

        let mut shared_flags = settings::builder();
        match opt_level {
            0 => {
                let _ = shared_flags.set("opt_level", "none");
            }
            _ => {
                let _ = shared_flags.set("opt_level", "speed_and_size");
            }
        }
        let _ = shared_flags.set("is_pic", "false");

        let flags = settings::Flags::new(shared_flags);
        let isa = isa_builder
            .finish(flags)
            .map_err(|e| BridgeError::Codegen(format!("failed to build ISA: {}", e)))?;

        let _ = target_triple; // We use native ISA, triple is for future cross-compilation

        let obj_builder =
            ObjectBuilder::new(isa, "tml_module", cranelift_module::default_libcall_names())
                .map_err(|e| {
                    BridgeError::Codegen(format!("failed to create object builder: {}", e))
                })?;
        let module = ObjectModule::new(obj_builder);

        Ok(Self {
            module,
            func_ids: HashMap::new(),
            struct_defs: HashMap::new(),
            enum_defs: HashMap::new(),
            runtime_names: std::collections::HashSet::new(),
        })
    }

    /// Populate the set of C runtime function names (no tml_ prefix).
    fn init_runtime_names(&mut self) {
        let names = [
            "print", "println", "panic", "assert_tml", "assert_tml_loc",
            "print_i32", "print_i64", "print_f32", "print_f64", "print_bool", "print_char",
            "str_len", "str_eq", "str_hash", "str_concat", "str_concat_opt",
            "str_concat_3", "str_concat_4", "str_concat_n",
            "str_substring", "str_slice", "str_contains", "str_starts_with", "str_ends_with",
            "str_to_upper", "str_to_lower", "str_trim", "str_char_at", "char_to_string",
            "time_ms", "time_us", "time_ns", "sleep_ms", "sleep_us",
            "elapsed_ms", "elapsed_us", "elapsed_ns",
            "mem_alloc", "mem_alloc_zeroed", "mem_realloc", "mem_free",
            "mem_copy", "mem_move", "mem_set", "mem_zero", "mem_compare", "mem_eq",
            "tml_set_output_suppressed", "tml_get_output_suppressed",
            "tml_run_should_panic", "tml_get_panic_message", "tml_panic_message_contains",
        ];
        for name in &names {
            self.runtime_names.insert(name.to_string());
        }
    }

    /// Translate a full MIR module. If `func_indices` is Some, only translate those functions (CGU mode).
    pub fn translate_module(
        &mut self,
        mir: &crate::mir_types::Module,
        func_indices: Option<&[usize]>,
    ) -> BridgeResult<()> {
        // Initialize runtime names before any declarations
        self.init_runtime_names();

        // Collect struct/enum definitions for layout computation
        for s in &mir.structs {
            self.struct_defs.insert(s.name.clone(), s.fields.clone());
        }
        for e in &mir.enums {
            self.enum_defs.insert(e.name.clone(), e.variants.clone());
        }

        // Phase 1: Declare all functions (so calls can reference any function)
        for func in &mir.functions {
            self.declare_function(func)?;
        }

        // Declare runtime functions
        self.declare_runtime_functions()?;

        // Phase 2: Define function bodies (only the requested subset in CGU mode)
        let indices: Vec<usize> = match func_indices {
            Some(idx) => idx.to_vec(),
            None => (0..mir.functions.len()).collect(),
        };

        let mut defined_funcs = std::collections::HashSet::new();
        for &i in &indices {
            if i < mir.functions.len() {
                let func = &mir.functions[i];
                // Skip duplicate function definitions
                if defined_funcs.contains(&func.name) {
                    continue;
                }
                defined_funcs.insert(func.name.clone());
                self.translate_function(func)?;
            }
        }

        Ok(())
    }

    /// Finish compilation and return the object file bytes.
    pub fn finish(self) -> BridgeResult<Vec<u8>> {
        let product = self.module.finish();
        let bytes = product.emit().map_err(|e| {
            BridgeError::Codegen(format!("failed to emit object file: {}", e))
        })?;
        Ok(bytes)
    }

    /// Map a MIR function name to the symbol name used in object files.
    /// User/library functions get "tml_" prefix; C runtime functions keep bare names.
    fn resolve_symbol_name(&self, mir_name: &str) -> String {
        // If it already has tml_ prefix, keep it
        if mir_name.starts_with("tml_") {
            return mir_name.to_string();
        }
        // C runtime functions don't get the prefix
        if self.runtime_names.contains(mir_name) {
            return mir_name.to_string();
        }
        // All other functions get tml_ prefix (matches LLVM codegen behavior)
        format!("tml_{}", mir_name)
    }

    fn declare_function(&mut self, func: &Function) -> BridgeResult<()> {
        let sig = self.build_signature(func);
        let symbol_name = self.resolve_symbol_name(&func.name);
        let linkage = if func.is_public || func.name == "main" || func.name == "tml_main" {
            Linkage::Export
        } else {
            Linkage::Local
        };

        // If already declared, try to re-declare with same signature (idempotent).
        // If signatures differ, use a disambiguated symbol name.
        if self.func_ids.contains_key(&func.name) {
            match self.module.declare_function(&symbol_name, linkage, &sig) {
                Ok(id) => {
                    // Same signature — update to latest func_id
                    self.func_ids.insert(func.name.clone(), id);
                    return Ok(());
                }
                Err(_) => {
                    // Different signature — create unique symbol with param/ret hash
                    let param_hash: usize = sig.params.iter()
                        .enumerate()
                        .map(|(i, p)| (i + 1) * p.value_type.bits() as usize)
                        .sum();
                    let ret_hash: usize = sig.returns.iter()
                        .map(|r| r.value_type.bits() as usize)
                        .sum();
                    let unique_sym = format!("{}${}p{}r{}", symbol_name, sig.params.len(), param_hash, ret_hash);
                    let id = self
                        .module
                        .declare_function(&unique_sym, linkage, &sig)
                        .map_err(|e| {
                            BridgeError::Codegen(format!(
                                "failed to declare function '{}' (unique: '{}'): {}",
                                func.name, unique_sym, e
                            ))
                        })?;
                    // Store under the MIR name (overwrites previous — latest wins)
                    self.func_ids.insert(func.name.clone(), id);
                    self.func_ids.insert(unique_sym, id);
                    return Ok(());
                }
            }
        }

        let id = self
            .module
            .declare_function(&symbol_name, linkage, &sig)
            .map_err(|e| {
                BridgeError::Codegen(format!(
                    "failed to declare function '{}' (symbol: '{}'): {}",
                    func.name, symbol_name, e
                ))
            })?;

        // Store under BOTH the MIR name and the symbol name for lookups
        self.func_ids.insert(func.name.clone(), id);
        if symbol_name != func.name {
            self.func_ids.insert(symbol_name, id);
        }
        Ok(())
    }

    fn build_signature(&self, func: &Function) -> cranelift_codegen::ir::Signature {
        let mut sig = self.module.make_signature();
        for param in &func.params {
            if let Some(cl_ty) = ty::mir_type_to_cranelift(&param.ty) {
                sig.params.push(AbiParam::new(cl_ty));
            }
        }
        if let Some(ret_ty) = ty::mir_type_to_cranelift(&func.return_type) {
            sig.returns.push(AbiParam::new(ret_ty));
        }
        sig
    }

    fn declare_runtime_functions(&mut self) -> BridgeResult<()> {
        // Declare external runtime functions from essential.h
        let rt_funcs: Vec<(&str, Vec<cranelift_codegen::ir::Type>, Option<cranelift_codegen::ir::Type>)> = vec![
            // I/O
            ("print", vec![POINTER_TYPE], None),
            ("println", vec![POINTER_TYPE], None),
            ("panic", vec![POINTER_TYPE], None),
            ("assert_tml", vec![types::I32, POINTER_TYPE], None),
            ("assert_tml_loc", vec![types::I32, POINTER_TYPE, POINTER_TYPE, types::I32], None),
            // Type-specific print
            ("print_i32", vec![types::I32], None),
            ("print_i64", vec![types::I64], None),
            ("print_f32", vec![types::F32], None),
            ("print_f64", vec![types::F64], None),
            ("print_bool", vec![types::I32], None),
            ("print_char", vec![types::I32], None),
            // String functions
            ("str_len", vec![POINTER_TYPE], Some(types::I32)),
            ("str_eq", vec![POINTER_TYPE, POINTER_TYPE], Some(types::I32)),
            ("str_hash", vec![POINTER_TYPE], Some(types::I32)),
            ("str_concat", vec![POINTER_TYPE, POINTER_TYPE], Some(POINTER_TYPE)),
            ("str_concat_opt", vec![POINTER_TYPE, POINTER_TYPE], Some(POINTER_TYPE)),
            ("str_concat_3", vec![POINTER_TYPE, POINTER_TYPE, POINTER_TYPE], Some(POINTER_TYPE)),
            ("str_concat_4", vec![POINTER_TYPE, POINTER_TYPE, POINTER_TYPE, POINTER_TYPE], Some(POINTER_TYPE)),
            ("str_concat_n", vec![POINTER_TYPE, types::I64], Some(POINTER_TYPE)),
            ("str_substring", vec![POINTER_TYPE, types::I32, types::I32], Some(POINTER_TYPE)),
            ("str_slice", vec![POINTER_TYPE, types::I64, types::I64], Some(POINTER_TYPE)),
            ("str_contains", vec![POINTER_TYPE, POINTER_TYPE], Some(types::I32)),
            ("str_starts_with", vec![POINTER_TYPE, POINTER_TYPE], Some(types::I32)),
            ("str_ends_with", vec![POINTER_TYPE, POINTER_TYPE], Some(types::I32)),
            ("str_to_upper", vec![POINTER_TYPE], Some(POINTER_TYPE)),
            ("str_to_lower", vec![POINTER_TYPE], Some(POINTER_TYPE)),
            ("str_trim", vec![POINTER_TYPE], Some(POINTER_TYPE)),
            ("str_char_at", vec![POINTER_TYPE, types::I32], Some(types::I32)),
            ("char_to_string", vec![types::I8], Some(POINTER_TYPE)),
            // Time
            ("time_ms", vec![], Some(types::I32)),
            ("time_us", vec![], Some(types::I64)),
            ("time_ns", vec![], Some(types::I64)),
            ("sleep_ms", vec![types::I32], None),
            ("sleep_us", vec![types::I64], None),
            ("elapsed_ms", vec![types::I32], Some(types::I32)),
            ("elapsed_us", vec![types::I64], Some(types::I64)),
            ("elapsed_ns", vec![types::I64], Some(types::I64)),
            // Memory
            ("mem_alloc", vec![types::I64], Some(POINTER_TYPE)),
            ("mem_alloc_zeroed", vec![types::I64], Some(POINTER_TYPE)),
            ("mem_realloc", vec![POINTER_TYPE, types::I64], Some(POINTER_TYPE)),
            ("mem_free", vec![POINTER_TYPE], None),
            ("mem_copy", vec![POINTER_TYPE, POINTER_TYPE, types::I64], None),
            ("mem_move", vec![POINTER_TYPE, POINTER_TYPE, types::I64], None),
            ("mem_set", vec![POINTER_TYPE, types::I32, types::I64], None),
            ("mem_zero", vec![POINTER_TYPE, types::I64], None),
            ("mem_compare", vec![POINTER_TYPE, POINTER_TYPE, types::I64], Some(types::I32)),
            ("mem_eq", vec![POINTER_TYPE, POINTER_TYPE, types::I64], Some(types::I32)),
            // Test/panic support
            ("tml_set_output_suppressed", vec![types::I32], None),
            ("tml_get_output_suppressed", vec![], Some(types::I32)),
            ("tml_run_should_panic", vec![POINTER_TYPE], Some(types::I32)),
            ("tml_get_panic_message", vec![], Some(POINTER_TYPE)),
            ("tml_panic_message_contains", vec![POINTER_TYPE], Some(types::I32)),
        ];

        for (name, params, ret) in &rt_funcs {
            if self.func_ids.contains_key(*name) {
                continue; // Already declared as a user function
            }
            let mut sig = self.module.make_signature();
            for &p in params {
                sig.params.push(AbiParam::new(p));
            }
            if let Some(r) = ret {
                sig.returns.push(AbiParam::new(*r));
            }
            let id = self
                .module
                .declare_function(name, Linkage::Import, &sig)
                .map_err(|e| {
                    BridgeError::Codegen(format!("failed to declare runtime function '{}': {}", name, e))
                })?;
            self.func_ids.insert(name.to_string(), id);
        }

        Ok(())
    }

    fn translate_function(&mut self, func: &Function) -> BridgeResult<()> {
        let func_id = *self.func_ids.get(&func.name).ok_or_else(|| {
            BridgeError::Translation(format!("function '{}' not declared", func.name))
        })?;

        // Skip empty functions (no blocks = no body to translate)
        if func.blocks.is_empty() {
            return Ok(());
        }

        let sig = self.build_signature(func);
        let mut cl_func = ClifFunc::with_name_signature(
            cranelift_codegen::ir::UserFuncName::user(0, func_id.as_u32()),
            sig,
        );

        let mut fb_ctx = FunctionBuilderContext::new();
        let mut builder = FunctionBuilder::new(&mut cl_func, &mut fb_ctx);

        {
            let mut ftx = FunctionTranslator::new(
                &mut builder,
                &mut self.func_ids,
                &self.struct_defs,
                &self.enum_defs,
                &mut self.module,
                func,
                &self.runtime_names,
            );
            ftx.translate()?;
        }
        builder.finalize();

        let mut ctx = cranelift_codegen::Context::for_function(cl_func);

        // Use catch_unwind to handle Cranelift internal panics gracefully
        // (e.g., "remove_constant_phis: entry block unknown")
        let define_result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.module.define_function(func_id, &mut ctx)
        }));

        match define_result {
            Ok(Ok(())) => Ok(()),
            Ok(Err(e)) => Err(BridgeError::Codegen(format!(
                "failed to define function '{}': {:?}",
                func.name, e
            ))),
            Err(panic_info) => {
                let msg = if let Some(s) = panic_info.downcast_ref::<&str>() {
                    s.to_string()
                } else if let Some(s) = panic_info.downcast_ref::<String>() {
                    s.clone()
                } else {
                    "unknown panic".to_string()
                };
                Err(BridgeError::Codegen(format!(
                    "PANIC in function '{}': {}",
                    func.name, msg
                )))
            }
        }
    }

    /// Generate Cranelift IR text for a module (without compiling to object).
    pub fn generate_ir_text(
        &mut self,
        mir: &crate::mir_types::Module,
    ) -> BridgeResult<String> {
        // Initialize runtime names before any declarations
        self.init_runtime_names();

        // Collect definitions
        for s in &mir.structs {
            self.struct_defs.insert(s.name.clone(), s.fields.clone());
        }
        for e in &mir.enums {
            self.enum_defs.insert(e.name.clone(), e.variants.clone());
        }

        for func in &mir.functions {
            self.declare_function(func)?;
        }
        self.declare_runtime_functions()?;

        let mut ir_text = String::new();
        for func in &mir.functions {
            let func_id = *self.func_ids.get(&func.name).unwrap();
            let sig = self.build_signature(func);
            let mut cl_func = ClifFunc::with_name_signature(
                cranelift_codegen::ir::UserFuncName::user(0, func_id.as_u32()),
                sig,
            );

            let mut fb_ctx = FunctionBuilderContext::new();
            let mut builder = FunctionBuilder::new(&mut cl_func, &mut fb_ctx);

            {
                let mut ftx = FunctionTranslator::new(
                    &mut builder,
                    &mut self.func_ids,
                    &self.struct_defs,
                    &self.enum_defs,
                    &mut self.module,
                    func,
                    &self.runtime_names,
                );
                ftx.translate()?;
            }
            builder.finalize();

            ir_text.push_str(&format!("; Function: {}\n", func.name));
            ir_text.push_str(&cl_func.display().to_string());
            ir_text.push('\n');
        }

        Ok(ir_text)
    }
}

/// Phi information collected in a pre-pass.
struct PhiInfo {
    /// block_id -> list of (result_value_id, incoming_pairs: Vec<(value_id, from_block_id)>)
    block_params: HashMap<u32, Vec<(ValueId, Vec<(ValueId, u32)>)>>,
}

/// Per-function translation state.
struct FunctionTranslator<'a, 'b> {
    builder: &'a mut FunctionBuilder<'b>,
    func_ids: &'a mut HashMap<String, FuncId>,
    struct_defs: &'a HashMap<String, Vec<StructField>>,
    enum_defs: &'a HashMap<String, Vec<EnumVariant>>,
    module: &'a mut ObjectModule,
    mir_func: &'a Function,
    /// C runtime function names (no tml_ prefix)
    runtime_names: &'a std::collections::HashSet<String>,
    /// Maps MIR ValueId → Cranelift Value
    values: HashMap<ValueId, ClifValue>,
    /// Maps MIR block id → Cranelift Block
    blocks: HashMap<u32, Block>,
    /// Maps alloca result_id → StackSlot
    alloca_slots: HashMap<ValueId, cranelift_codegen::ir::StackSlot>,
    /// Phi info (block parameters)
    phi_info: PhiInfo,
    /// String constants data section
    string_data: HashMap<String, cranelift_module::DataId>,
    /// Maps MIR ValueId → inferred Cranelift type (from instruction analysis)
    value_types: HashMap<ValueId, cranelift_codegen::ir::Type>,
}

fn make_stack_slot(size: u32) -> StackSlotData {
    StackSlotData::new(StackSlotKind::ExplicitSlot, size, 0)
}

impl<'a, 'b> FunctionTranslator<'a, 'b> {
    fn new(
        builder: &'a mut FunctionBuilder<'b>,
        func_ids: &'a mut HashMap<String, FuncId>,
        struct_defs: &'a HashMap<String, Vec<StructField>>,
        enum_defs: &'a HashMap<String, Vec<EnumVariant>>,
        module: &'a mut ObjectModule,
        mir_func: &'a Function,
        runtime_names: &'a std::collections::HashSet<String>,
    ) -> Self {
        Self {
            builder,
            func_ids,
            struct_defs,
            enum_defs,
            module,
            mir_func,
            runtime_names,
            values: HashMap::new(),
            blocks: HashMap::new(),
            alloca_slots: HashMap::new(),
            phi_info: PhiInfo {
                block_params: HashMap::new(),
            },
            string_data: HashMap::new(),
            value_types: HashMap::new(),
        }
    }

    /// Resolve a MIR function name to the linker symbol name.
    fn resolve_symbol_name(&self, mir_name: &str) -> String {
        if mir_name.starts_with("tml_") {
            return mir_name.to_string();
        }
        if self.runtime_names.contains(mir_name) {
            return mir_name.to_string();
        }
        format!("tml_{}", mir_name)
    }

    fn translate(&mut self) -> BridgeResult<()> {
        // Pre-pass: determine types for all values (needed for phi type inference)
        self.collect_value_types();

        // Pre-pass: collect phi instructions to convert to block parameters
        self.collect_phi_info();

        // Create Cranelift blocks
        for block in &self.mir_func.blocks {
            let cl_block = self.builder.create_block();
            self.blocks.insert(block.id, cl_block);
        }

        // Add block parameters for phi nodes
        for block in &self.mir_func.blocks {
            let cl_block = self.blocks[&block.id];
            if let Some(phis) = self.phi_info.block_params.get(&block.id) {
                for (result_id, _incoming) in phis {
                    let param_type = self.infer_phi_type(*result_id);
                    let cl_param = self.builder.append_block_param(cl_block, param_type);
                    self.values.insert(*result_id, cl_param);
                }
            }
        }

        // Guard against empty functions (no blocks)
        if self.mir_func.blocks.is_empty() {
            return Ok(());
        }

        // Entry block receives function parameters
        let entry_block = self.blocks[&self.mir_func.blocks[0].id];
        self.builder.append_block_params_for_function_params(entry_block);

        // Map function params to value IDs
        let param_vals = self.builder.block_params(entry_block);
        // Block params for phis come first, then function params
        let phi_count = self
            .phi_info
            .block_params
            .get(&self.mir_func.blocks[0].id)
            .map_or(0, |v| v.len());
        for (i, param) in self.mir_func.params.iter().enumerate() {
            if phi_count + i < param_vals.len() {
                self.values.insert(param.value_id, param_vals[phi_count + i]);
            }
        }

        self.builder.switch_to_block(entry_block);

        // Translate each block
        for (block_idx, block) in self.mir_func.blocks.iter().enumerate() {
            if block_idx > 0 {
                let cl_block = self.blocks[&block.id];
                self.builder.switch_to_block(cl_block);
            }

            // Translate instructions (skip phi nodes — already handled as block params)
            for inst_data in &block.instructions {
                if matches!(&inst_data.inst, Instruction::Phi { .. }) {
                    continue;
                }
                self.translate_instruction(inst_data)?;
            }

            // Translate terminator
            if let Some(term) = &block.terminator {
                self.translate_terminator(term, block.id)?;
            }
        }

        // Seal all blocks AFTER all branches have been emitted.
        // This is required because Cranelift needs to know all predecessors
        // of a block before it can be sealed (for SSA construction).
        self.builder.seal_all_blocks();

        Ok(())
    }

    /// Pre-pass: collect all phi instructions and group by block.
    fn collect_phi_info(&mut self) {
        for block in &self.mir_func.blocks {
            let mut phis = Vec::new();
            for inst in &block.instructions {
                if let Instruction::Phi { incoming } = &inst.inst {
                    // Convert Vec<(Value, u32)> to Vec<(ValueId, u32)>
                    let converted: Vec<(ValueId, u32)> =
                        incoming.iter().map(|(v, b)| (v.id, *b)).collect();
                    phis.push((inst.result, converted));
                }
            }
            if !phis.is_empty() {
                self.phi_info.block_params.insert(block.id, phis);
            }
        }
    }

    /// Infer the Cranelift type for a phi node by looking at incoming values.
    fn infer_phi_type(&self, result_id: ValueId) -> cranelift_codegen::ir::Type {
        // Look at phi incoming values to determine the type
        for block in &self.mir_func.blocks {
            for inst in &block.instructions {
                if inst.result == result_id {
                    if let Instruction::Phi { incoming } = &inst.inst {
                        // Use the type of the first incoming value
                        for (val, _block_id) in incoming {
                            if let Some(&ty) = self.value_types.get(&val.id) {
                                return ty;
                            }
                        }
                    }
                }
            }
        }
        // Fallback to I64
        types::I64
    }

    /// Pre-pass: scan all instructions to build a value_id → type map.
    fn collect_value_types(&mut self) {
        // Map function parameters
        for param in &self.mir_func.params {
            if let Some(cl_ty) = ty::mir_type_to_cranelift(&param.ty) {
                self.value_types.insert(param.value_id, cl_ty);
            } else {
                // Unit type or unmappable — skip
            }
        }

        // First pass: collect alloca types (alloca result_id → the type being allocated)
        let mut alloca_types: HashMap<ValueId, cranelift_codegen::ir::Type> = HashMap::new();
        for block in &self.mir_func.blocks {
            for inst in &block.instructions {
                if let Instruction::Alloca { alloc_type, .. } = &inst.inst {
                    if let Some(cl_ty) = ty::mir_type_to_cranelift(alloc_type) {
                        alloca_types.insert(inst.result, cl_ty);
                    }
                }
            }
        }

        // Scan all instructions to infer result types
        for block in &self.mir_func.blocks {
            for inst in &block.instructions {
                let result_id = inst.result;
                let inferred_ty = match &inst.inst {
                    Instruction::Constant(c) => match c {
                        Constant::Int { bit_width, .. } => match bit_width {
                            8 => Some(types::I8),
                            16 => Some(types::I16),
                            32 => Some(types::I32),
                            64 => Some(types::I64),
                            128 => Some(types::I128),
                            _ => Some(types::I64),
                        },
                        Constant::Float { is_f64, .. } => {
                            if *is_f64 { Some(types::F64) } else { Some(types::F32) }
                        },
                        Constant::Bool(_) => Some(types::I8),
                        Constant::String(_) => Some(POINTER_TYPE),
                        Constant::Unit => None,
                    },
                    Instruction::Binary { op, left, right } => {
                        // Comparison ops always return I8 (bool)
                        if op.is_comparison() {
                            Some(types::I8)
                        } else {
                            // Result type matches the wider operand type
                            let l = self.value_types.get(&left.id).copied();
                            let r = self.value_types.get(&right.id).copied();
                            match (l, r) {
                                (Some(lt), Some(rt)) if lt.is_int() && rt.is_int() => {
                                    Some(if lt.bytes() >= rt.bytes() { lt } else { rt })
                                },
                                (Some(lt), _) => Some(lt),
                                (_, Some(rt)) => Some(rt),
                                _ => None,
                            }
                        }
                    },
                    Instruction::Unary { operand, .. } => {
                        self.value_types.get(&operand.id).copied()
                    },
                    Instruction::Call { return_type, .. } | Instruction::MethodCall { return_type, .. } => {
                        ty::mir_type_to_cranelift(return_type)
                    },
                    Instruction::Cast { target_type, .. } => {
                        ty::mir_type_to_cranelift(target_type)
                    },
                    Instruction::Select { true_val, false_val, .. } => {
                        let l = self.value_types.get(&true_val.id).copied();
                        let r = self.value_types.get(&false_val.id).copied();
                        match (l, r) {
                            (Some(lt), Some(rt)) if lt.is_int() && rt.is_int() => {
                                Some(if lt.bytes() >= rt.bytes() { lt } else { rt })
                            },
                            (Some(lt), _) => Some(lt),
                            (_, Some(rt)) => Some(rt),
                            _ => None,
                        }
                    },
                    Instruction::Alloca { .. } => Some(POINTER_TYPE),
                    Instruction::Load { ptr } => {
                        // If loading from an alloca, use the alloca's element type
                        alloca_types.get(&ptr.id).copied().or(Some(types::I64))
                    },
                    Instruction::Store { .. } => None,
                    Instruction::Gep { .. } => Some(POINTER_TYPE),
                    Instruction::ExtractValue { .. } => Some(types::I64),
                    Instruction::InsertValue { .. } => Some(POINTER_TYPE),
                    Instruction::StructInit { .. } => Some(POINTER_TYPE),
                    Instruction::EnumInit { .. } => Some(POINTER_TYPE),
                    Instruction::TupleInit { .. } => Some(POINTER_TYPE),
                    Instruction::ArrayInit { .. } => Some(POINTER_TYPE),
                    Instruction::Phi { incoming } => {
                        // Try to get type from incoming values
                        incoming.iter()
                            .find_map(|(v, _)| self.value_types.get(&v.id).copied())
                    },
                    _ => Some(types::I64),
                };
                if let Some(t) = inferred_ty {
                    self.value_types.insert(result_id, t);
                }
            }
        }
    }

    fn get_value(&mut self, val: &Value) -> BridgeResult<ClifValue> {
        // u32::MAX is a sentinel for "no value" in some MIR paths
        if val.id == u32::MAX {
            return Ok(self.builder.ins().iconst(types::I64, 0));
        }
        if let Some(&v) = self.values.get(&val.id) {
            return Ok(v);
        }
        // Value not found — this can happen for forward references or
        // values from unreachable blocks. Produce a zero constant with the
        // inferred type (or I64 default) instead of failing hard.
        let fallback_ty = self.value_types.get(&val.id).copied().unwrap_or(types::I64);
        if fallback_ty.is_int() {
            Ok(self.builder.ins().iconst(fallback_ty, 0))
        } else if fallback_ty == types::F32 {
            Ok(self.builder.ins().f32const(0.0))
        } else if fallback_ty == types::F64 {
            Ok(self.builder.ins().f64const(0.0))
        } else {
            Ok(self.builder.ins().iconst(types::I64, 0))
        }
    }

    fn translate_instruction(&mut self, inst_data: &InstructionData) -> BridgeResult<()> {
        let result_id = inst_data.result;
        match &inst_data.inst {
            Instruction::Constant(constant) => {
                let val = self.translate_constant(constant)?;
                self.values.insert(result_id, val);
            }

            Instruction::Binary { op, left, right } => {
                let lhs = self.get_value(left)?;
                let rhs = self.get_value(right)?;
                let val = self.translate_binary(*op, lhs, rhs)?;
                self.values.insert(result_id, val);
            }

            Instruction::Unary { op, operand } => {
                let operand_val = self.get_value(operand)?;
                let val = self.translate_unary(*op, operand_val)?;
                self.values.insert(result_id, val);
            }

            Instruction::Alloca { name: _, alloc_type } => {
                let size = ty::type_size(alloc_type);
                let slot = self.builder.create_sized_stack_slot(make_stack_slot(size));
                self.alloca_slots.insert(result_id, slot);
                let addr = self.builder.ins().stack_addr(POINTER_TYPE, slot, 0);
                self.values.insert(result_id, addr);
            }

            Instruction::Load { ptr } => {
                let ptr_val = self.get_value(ptr)?;
                // Use the pre-computed type for this load result if available,
                // otherwise default to I64
                let load_ty = self.value_types.get(&result_id).copied().unwrap_or(types::I64);
                if let Some(&slot) = self.alloca_slots.get(&ptr.id) {
                    let val = self.builder.ins().stack_load(load_ty, slot, 0);
                    self.values.insert(result_id, val);
                } else {
                    let val = self.builder.ins().load(load_ty, MemFlags::new(), ptr_val, 0);
                    self.values.insert(result_id, val);
                }
            }

            Instruction::Store { ptr, value } => {
                let mut val = self.get_value(value)?;
                if let Some(&slot) = self.alloca_slots.get(&ptr.id) {
                    // Coerce value to match load type (stored and loaded types must match)
                    let val_ty = self.builder.func.dfg.value_type(val);
                    let slot_size = self.builder.func.sized_stack_slots[slot].size;
                    let expected_ty = match slot_size {
                        1 => types::I8,
                        2 => types::I16,
                        4 => types::I32,
                        _ => types::I64,
                    };
                    if val_ty != expected_ty && val_ty.is_int() && expected_ty.is_int() {
                        val = if val_ty.bytes() < expected_ty.bytes() {
                            self.builder.ins().sextend(expected_ty, val)
                        } else {
                            self.builder.ins().ireduce(expected_ty, val)
                        };
                    }
                    self.builder.ins().stack_store(val, slot, 0);
                } else {
                    let ptr_v = self.get_value(ptr)?;
                    self.builder.ins().store(MemFlags::new(), val, ptr_v, 0);
                }
            }

            Instruction::Call {
                func_name,
                args,
                return_type,
            } => {
                let call_val = self.translate_call(func_name, args, return_type)?;
                if let Some(v) = call_val {
                    self.values.insert(result_id, v);
                }
            }

            Instruction::MethodCall {
                receiver,
                method_name,
                args,
                return_type,
            } => {
                let mut all_args = vec![receiver.clone()];
                all_args.extend_from_slice(args);
                let call_val = self.translate_call(method_name, &all_args, return_type)?;
                if let Some(v) = call_val {
                    self.values.insert(result_id, v);
                }
            }

            Instruction::Cast {
                kind,
                operand,
                target_type,
            } => {
                let operand_val = self.get_value(operand)?;
                let val = self.translate_cast(*kind, operand_val, target_type)?;
                self.values.insert(result_id, val);
            }

            Instruction::Select {
                condition,
                true_val,
                false_val,
            } => {
                let cond = self.get_value(condition)?;
                let mut tv = self.get_value(true_val)?;
                let mut fv = self.get_value(false_val)?;
                // Coerce true/false values to same type
                let tv_ty = self.builder.func.dfg.value_type(tv);
                let fv_ty = self.builder.func.dfg.value_type(fv);
                if tv_ty != fv_ty && tv_ty.is_int() && fv_ty.is_int() {
                    let target = if tv_ty.bytes() >= fv_ty.bytes() { tv_ty } else { fv_ty };
                    if tv_ty != target {
                        tv = self.builder.ins().sextend(target, tv);
                    }
                    if fv_ty != target {
                        fv = self.builder.ins().sextend(target, fv);
                    }
                }
                let val = self.builder.ins().select(cond, tv, fv);
                self.values.insert(result_id, val);
            }

            Instruction::StructInit {
                struct_name,
                fields,
            } => {
                let val = self.translate_struct_init(struct_name, fields)?;
                self.values.insert(result_id, val);
            }

            Instruction::EnumInit {
                enum_name,
                variant_name,
                payload,
            } => {
                let val = self.translate_enum_init(enum_name, variant_name, payload)?;
                self.values.insert(result_id, val);
            }

            Instruction::TupleInit { elements } => {
                let val = self.translate_tuple_init(elements)?;
                self.values.insert(result_id, val);
            }

            Instruction::ArrayInit {
                element_type,
                elements,
            } => {
                let val = self.translate_array_init(element_type, elements)?;
                self.values.insert(result_id, val);
            }

            Instruction::Gep { base, indices } => {
                let val = self.translate_gep(base, indices)?;
                self.values.insert(result_id, val);
            }

            Instruction::ExtractValue { aggregate, indices } => {
                let val = self.translate_extract_value(aggregate, indices)?;
                self.values.insert(result_id, val);
            }

            Instruction::InsertValue {
                aggregate,
                value,
                indices,
            } => {
                let val = self.translate_insert_value(aggregate, value, indices)?;
                self.values.insert(result_id, val);
            }

            Instruction::Await { .. } => {
                return Err(BridgeError::UnsupportedInstruction(
                    "await not supported in Cranelift backend".into(),
                ));
            }

            Instruction::ClosureInit {
                func_name,
                captures,
                ..
            } => {
                let val = self.translate_closure_init(func_name, captures)?;
                self.values.insert(result_id, val);
            }

            Instruction::Phi { .. } => {
                // Already handled in pre-pass
            }
        }

        Ok(())
    }

    fn translate_constant(&mut self, constant: &Constant) -> BridgeResult<ClifValue> {
        match constant {
            Constant::Int {
                value,
                bit_width,
                is_signed: _,
            } => {
                let ty = match bit_width {
                    8 => types::I8,
                    16 => types::I16,
                    32 => types::I32,
                    64 => types::I64,
                    128 => types::I128,
                    _ => types::I64,
                };
                Ok(self.builder.ins().iconst(ty, *value))
            }
            Constant::Float { value, is_f64 } => {
                if *is_f64 {
                    Ok(self.builder.ins().f64const(*value))
                } else {
                    Ok(self.builder.ins().f32const(*value as f32))
                }
            }
            Constant::Bool(b) => {
                Ok(self.builder.ins().iconst(types::I8, if *b { 1 } else { 0 }))
            }
            Constant::String(s) => {
                self.translate_string_constant(s)
            }
            Constant::Unit => {
                Ok(self.builder.ins().iconst(types::I64, 0))
            }
        }
    }

    fn translate_string_constant(&mut self, s: &str) -> BridgeResult<ClifValue> {
        if let Some(&data_id) = self.string_data.get(s) {
            let gv = self
                .module
                .declare_data_in_func(data_id, self.builder.func);
            return Ok(self.builder.ins().symbol_value(POINTER_TYPE, gv));
        }

        let name = format!(".str.{}.{}", self.mir_func.name, self.string_data.len());
        let data_id = self
            .module
            .declare_data(&name, Linkage::Local, false, false)
            .map_err(|e| BridgeError::Codegen(format!("failed to declare string data: {}", e)))?;

        let mut data_desc = cranelift_module::DataDescription::new();
        let mut bytes = s.as_bytes().to_vec();
        bytes.push(0); // null terminator
        data_desc.define(bytes.into_boxed_slice());
        self.module
            .define_data(data_id, &data_desc)
            .map_err(|e| BridgeError::Codegen(format!("failed to define string data: {}", e)))?;

        self.string_data.insert(s.to_string(), data_id);

        let gv = self
            .module
            .declare_data_in_func(data_id, self.builder.func);
        Ok(self.builder.ins().symbol_value(POINTER_TYPE, gv))
    }

    fn translate_binary(
        &mut self,
        op: BinOp,
        lhs: ClifValue,
        rhs: ClifValue,
    ) -> BridgeResult<ClifValue> {
        let lhs_ty = self.builder.func.dfg.value_type(lhs);
        let rhs_ty = self.builder.func.dfg.value_type(rhs);
        let lhs_is_float = lhs_ty == types::F32 || lhs_ty == types::F64;
        let rhs_is_float = rhs_ty == types::F32 || rhs_ty == types::F64;
        let is_float = lhs_is_float || rhs_is_float;

        // Coerce operands to same type if they differ
        let (lhs, rhs) = if lhs_ty != rhs_ty {
            if lhs_is_float && rhs_is_float {
                // Both float but different precision — promote to wider
                let target = if lhs_ty == types::F64 { types::F64 } else { rhs_ty };
                let l = if lhs_ty == target { lhs } else { self.builder.ins().fpromote(target, lhs) };
                let r = if rhs_ty == target { rhs } else { self.builder.ins().fpromote(target, rhs) };
                (l, r)
            } else if lhs_is_float && !rhs_is_float {
                // LHS float, RHS int — convert RHS int to float
                let r = self.builder.ins().fcvt_from_sint(lhs_ty, rhs);
                (lhs, r)
            } else if !lhs_is_float && rhs_is_float {
                // LHS int, RHS float — convert LHS int to float
                let l = self.builder.ins().fcvt_from_sint(rhs_ty, lhs);
                (l, rhs)
            } else if lhs_ty.is_int() && rhs_ty.is_int() {
                // Both int but different widths
                let target = if lhs_ty.bytes() >= rhs_ty.bytes() { lhs_ty } else { rhs_ty };
                let l = if lhs_ty == target { lhs } else { self.builder.ins().sextend(target, lhs) };
                let r = if rhs_ty == target { rhs } else { self.builder.ins().sextend(target, rhs) };
                (l, r)
            } else {
                (lhs, rhs)
            }
        } else {
            (lhs, rhs)
        };

        let val = match op {
            BinOp::Add => {
                if is_float { self.builder.ins().fadd(lhs, rhs) }
                else { self.builder.ins().iadd(lhs, rhs) }
            }
            BinOp::Sub => {
                if is_float { self.builder.ins().fsub(lhs, rhs) }
                else { self.builder.ins().isub(lhs, rhs) }
            }
            BinOp::Mul => {
                if is_float { self.builder.ins().fmul(lhs, rhs) }
                else { self.builder.ins().imul(lhs, rhs) }
            }
            BinOp::Div => {
                if is_float { self.builder.ins().fdiv(lhs, rhs) }
                else { self.builder.ins().sdiv(lhs, rhs) }
            }
            BinOp::Mod => {
                if is_float {
                    return Err(BridgeError::UnsupportedInstruction(
                        "float modulo not directly supported".into(),
                    ));
                } else {
                    self.builder.ins().srem(lhs, rhs)
                }
            }
            BinOp::Eq => {
                if is_float { self.builder.ins().fcmp(FloatCC::Equal, lhs, rhs) }
                else { self.builder.ins().icmp(IntCC::Equal, lhs, rhs) }
            }
            BinOp::Ne => {
                if is_float { self.builder.ins().fcmp(FloatCC::NotEqual, lhs, rhs) }
                else { self.builder.ins().icmp(IntCC::NotEqual, lhs, rhs) }
            }
            BinOp::Lt => {
                if is_float { self.builder.ins().fcmp(FloatCC::LessThan, lhs, rhs) }
                else { self.builder.ins().icmp(IntCC::SignedLessThan, lhs, rhs) }
            }
            BinOp::Le => {
                if is_float { self.builder.ins().fcmp(FloatCC::LessThanOrEqual, lhs, rhs) }
                else { self.builder.ins().icmp(IntCC::SignedLessThanOrEqual, lhs, rhs) }
            }
            BinOp::Gt => {
                if is_float { self.builder.ins().fcmp(FloatCC::GreaterThan, lhs, rhs) }
                else { self.builder.ins().icmp(IntCC::SignedGreaterThan, lhs, rhs) }
            }
            BinOp::Ge => {
                if is_float { self.builder.ins().fcmp(FloatCC::GreaterThanOrEqual, lhs, rhs) }
                else { self.builder.ins().icmp(IntCC::SignedGreaterThanOrEqual, lhs, rhs) }
            }
            BinOp::And => self.builder.ins().band(lhs, rhs),
            BinOp::Or => self.builder.ins().bor(lhs, rhs),
            BinOp::BitAnd => self.builder.ins().band(lhs, rhs),
            BinOp::BitOr => self.builder.ins().bor(lhs, rhs),
            BinOp::BitXor => self.builder.ins().bxor(lhs, rhs),
            BinOp::Shl => self.builder.ins().ishl(lhs, rhs),
            BinOp::Shr => self.builder.ins().sshr(lhs, rhs),
        };

        Ok(val)
    }

    fn translate_unary(
        &mut self,
        op: UnaryOp,
        operand: ClifValue,
    ) -> BridgeResult<ClifValue> {
        let ty = self.builder.func.dfg.value_type(operand);
        let is_float = ty == types::F32 || ty == types::F64;

        let val = match op {
            UnaryOp::Neg => {
                if is_float { self.builder.ins().fneg(operand) }
                else { self.builder.ins().ineg(operand) }
            }
            UnaryOp::Not => {
                let one = self.builder.ins().iconst(ty, 1);
                self.builder.ins().bxor(operand, one)
            }
            UnaryOp::BitNot => self.builder.ins().bnot(operand),
        };

        Ok(val)
    }

    fn translate_call(
        &mut self,
        func_name: &str,
        args: &[Value],
        return_type: &MirType,
    ) -> BridgeResult<Option<ClifValue>> {
        let func_id = if let Some(&id) = self.func_ids.get(func_name) {
            id
        } else {
            // Try with tml_ prefix (MIR uses bare names, declarations use tml_ prefix)
            let symbol_name = self.resolve_symbol_name(func_name);
            if let Some(&id) = self.func_ids.get(&symbol_name) {
                id
            } else {
                // Unknown function — declare as import with inferred signature
                // Use resolved symbol name (tml_ prefix for user/lib funcs)
                let mut sig = self.module.make_signature();
                for _ in args {
                    sig.params.push(AbiParam::new(types::I64));
                }
                if let Some(ret_ty) = ty::mir_type_to_cranelift(return_type) {
                    sig.returns.push(AbiParam::new(ret_ty));
                }
                match self.module.declare_function(&symbol_name, Linkage::Import, &sig) {
                    Ok(id) => {
                        // Store the new func_id for future lookups
                        self.func_ids.insert(func_name.to_string(), id);
                        if symbol_name != func_name {
                            self.func_ids.insert(symbol_name, id);
                        }
                        id
                    }
                    Err(_) => {
                        // Signature incompatible — the function was already declared
                        // with a different signature (different monomorphization).
                        // Declare a unique import with disambiguated symbol name.
                        let param_hash: usize = sig.params.iter()
                            .enumerate()
                            .map(|(i, p)| (i + 1) * p.value_type.bits() as usize)
                            .sum();
                        let ret_hash: usize = sig.returns.iter()
                            .map(|r| r.value_type.bits() as usize)
                            .sum();
                        let unique_sym = format!("{}${}p{}r{}", symbol_name, sig.params.len(), param_hash, ret_hash);
                        self.module
                            .declare_function(&unique_sym, Linkage::Import, &sig)
                            .map_err(|e| {
                                BridgeError::Codegen(format!(
                                    "failed to declare function '{}' (symbol: '{}'): {}",
                                    func_name, symbol_name, e
                                ))
                            })?
                    }
                }
            }
        };

        let local_callee = self
            .module
            .declare_func_in_func(func_id, self.builder.func);

        // Get the expected parameter types from the function signature
        let sig = self.builder.func.dfg.ext_funcs[local_callee].signature;
        let expected_types: Vec<cranelift_codegen::ir::Type> = self.builder.func.dfg.signatures[sig]
            .params
            .iter()
            .map(|p| p.value_type)
            .collect();

        let mut arg_vals = Vec::with_capacity(args.len());
        for (i, arg) in args.iter().enumerate() {
            let mut val = self.get_value(arg)?;
            let actual_ty = self.builder.func.dfg.value_type(val);

            // Coerce argument type to match expected parameter type
            if i < expected_types.len() {
                let expected_ty = expected_types[i];
                if actual_ty != expected_ty {
                    let actual_is_int = actual_ty.is_int();
                    let expected_is_int = expected_ty.is_int();
                    let actual_is_float = actual_ty == types::F32 || actual_ty == types::F64;
                    let expected_is_float = expected_ty == types::F32 || expected_ty == types::F64;
                    if actual_is_int && expected_is_int {
                        if actual_ty.bytes() < expected_ty.bytes() {
                            val = self.builder.ins().sextend(expected_ty, val);
                        } else if actual_ty.bytes() > expected_ty.bytes() {
                            val = self.builder.ins().ireduce(expected_ty, val);
                        }
                    } else if actual_is_float && expected_is_int {
                        // Convert float to integer
                        val = self.builder.ins().fcvt_to_sint(expected_ty, val);
                    } else if actual_is_int && expected_is_float {
                        // Convert integer to float
                        val = self.builder.ins().fcvt_from_sint(expected_ty, val);
                    } else if actual_is_float && expected_is_float {
                        // Float precision coercion
                        if actual_ty == types::F32 && expected_ty == types::F64 {
                            val = self.builder.ins().fpromote(types::F64, val);
                        } else if actual_ty == types::F64 && expected_ty == types::F32 {
                            val = self.builder.ins().fdemote(types::F32, val);
                        }
                    }
                }
            }
            arg_vals.push(val);
        }

        let call = self.builder.ins().call(local_callee, &arg_vals);
        let results = self.builder.inst_results(call);

        if results.is_empty() {
            Ok(None)
        } else {
            Ok(Some(results[0]))
        }
    }

    fn translate_cast(
        &mut self,
        kind: CastKind,
        operand: ClifValue,
        target_type: &MirType,
    ) -> BridgeResult<ClifValue> {
        let target_cl = ty::mir_type_to_cranelift(target_type).unwrap_or(types::I64);
        let src_ty = self.builder.func.dfg.value_type(operand);

        let val = match kind {
            CastKind::Bitcast => {
                if src_ty == target_cl {
                    operand
                } else if src_ty.bytes() == target_cl.bytes() {
                    self.builder.ins().bitcast(target_cl, MemFlags::new(), operand)
                } else if src_ty.is_int() && target_cl.is_int() {
                    // Different-sized integers: use extend/reduce instead of bitcast
                    if src_ty.bytes() < target_cl.bytes() {
                        self.builder.ins().uextend(target_cl, operand)
                    } else {
                        self.builder.ins().ireduce(target_cl, operand)
                    }
                } else {
                    // Fallback: try raw_bitcast for same-register-class types
                    self.builder.ins().bitcast(target_cl, MemFlags::new(), operand)
                }
            }
            CastKind::Trunc => self.builder.ins().ireduce(target_cl, operand),
            CastKind::ZExt => self.builder.ins().uextend(target_cl, operand),
            CastKind::SExt => self.builder.ins().sextend(target_cl, operand),
            CastKind::FPTrunc => self.builder.ins().fdemote(target_cl, operand),
            CastKind::FPExt => self.builder.ins().fpromote(target_cl, operand),
            CastKind::FPToSI => self.builder.ins().fcvt_to_sint(target_cl, operand),
            CastKind::FPToUI => self.builder.ins().fcvt_to_uint(target_cl, operand),
            CastKind::SIToFP => self.builder.ins().fcvt_from_sint(target_cl, operand),
            CastKind::UIToFP => self.builder.ins().fcvt_from_uint(target_cl, operand),
            CastKind::PtrToInt => {
                if src_ty == target_cl { operand }
                else if src_ty.bytes() > target_cl.bytes() { self.builder.ins().ireduce(target_cl, operand) }
                else { self.builder.ins().uextend(target_cl, operand) }
            }
            CastKind::IntToPtr => {
                if src_ty == POINTER_TYPE { operand }
                else if src_ty.bytes() < POINTER_TYPE.bytes() { self.builder.ins().uextend(POINTER_TYPE, operand) }
                else { self.builder.ins().ireduce(POINTER_TYPE, operand) }
            }
        };

        Ok(val)
    }

    fn translate_terminator(&mut self, term: &Terminator, current_block_id: u32) -> BridgeResult<()> {
        match term {
            Terminator::Return { value } => {
                if let Some(val) = value {
                    let mut v = self.get_value(val)?;
                    // Coerce return value to match function signature
                    let actual_ty = self.builder.func.dfg.value_type(v);
                    if let Some(ret_param) = self.builder.func.signature.returns.first() {
                        let expected_ty = ret_param.value_type;
                        if actual_ty != expected_ty {
                            let actual_is_int = actual_ty.is_int();
                            let expected_is_int = expected_ty.is_int();
                            let actual_is_float = actual_ty == types::F32 || actual_ty == types::F64;
                            let expected_is_float = expected_ty == types::F32 || expected_ty == types::F64;
                            if actual_is_int && expected_is_int {
                                if actual_ty.bytes() < expected_ty.bytes() {
                                    v = self.builder.ins().sextend(expected_ty, v);
                                } else if actual_ty.bytes() > expected_ty.bytes() {
                                    v = self.builder.ins().ireduce(expected_ty, v);
                                }
                            } else if actual_is_float && expected_is_int {
                                v = self.builder.ins().fcvt_to_sint(expected_ty, v);
                            } else if actual_is_int && expected_is_float {
                                v = self.builder.ins().fcvt_from_sint(expected_ty, v);
                            } else if actual_is_float && expected_is_float {
                                if actual_ty == types::F32 && expected_ty == types::F64 {
                                    v = self.builder.ins().fpromote(types::F64, v);
                                } else if actual_ty == types::F64 && expected_ty == types::F32 {
                                    v = self.builder.ins().fdemote(types::F32, v);
                                }
                            }
                        }
                    }
                    self.builder.ins().return_(&[v]);
                } else {
                    self.builder.ins().return_(&[]);
                }
            }
            Terminator::Branch { target } => {
                let target_block = self.blocks[target];
                let args = self.collect_phi_args(*target, current_block_id)?;
                self.builder.ins().jump(target_block, &args);
            }
            Terminator::CondBranch {
                condition,
                true_block,
                false_block,
            } => {
                let cond = self.get_value(condition)?;
                let tb = self.blocks[true_block];
                let fb = self.blocks[false_block];
                let true_args = self.collect_phi_args(*true_block, current_block_id)?;
                let false_args = self.collect_phi_args(*false_block, current_block_id)?;
                self.builder.ins().brif(cond, tb, &true_args, fb, &false_args);
            }
            Terminator::Switch {
                discriminant,
                cases,
                default_block,
            } => {
                let disc = self.get_value(discriminant)?;
                let default_bl = self.blocks[default_block];

                let mut switch = cranelift_frontend::Switch::new();
                for (case_val, block_id) in cases {
                    let target = self.blocks[block_id];
                    switch.set_entry(*case_val as u128, target);
                }
                switch.emit(self.builder, disc, default_bl);
            }
            Terminator::Unreachable => {
                self.builder.ins().trap(TrapCode::unwrap_user(0));
            }
        }

        Ok(())
    }

    /// Collect the values to pass as block arguments for phi nodes in the target block.
    /// Handles type coercion when incoming value type doesn't match block parameter type.
    fn collect_phi_args(
        &mut self,
        target_block_id: u32,
        from_block_id: u32,
    ) -> BridgeResult<Vec<BlockArg>> {
        let mut args = Vec::new();

        // Get the expected parameter types for the target block
        let target_block = self.blocks[&target_block_id];
        let param_types: Vec<cranelift_codegen::ir::Type> = self
            .builder
            .block_params(target_block)
            .iter()
            .map(|&p| self.builder.func.dfg.value_type(p))
            .collect();

        if let Some(phis) = self.phi_info.block_params.get(&target_block_id) {
            for (phi_idx, (_result_id, incoming)) in phis.iter().enumerate() {
                let expected_ty = param_types.get(phi_idx).copied().unwrap_or(types::I64);
                let mut found = false;
                for (val_id, block_id) in incoming {
                    if *block_id == from_block_id {
                        if let Some(&v) = self.values.get(val_id) {
                            // Coerce type if needed
                            let actual_ty = self.builder.func.dfg.value_type(v);
                            let coerced = if actual_ty == expected_ty {
                                v
                            } else if actual_ty.is_int() && expected_ty.is_int() {
                                if actual_ty.bytes() < expected_ty.bytes() {
                                    self.builder.ins().sextend(expected_ty, v)
                                } else {
                                    self.builder.ins().ireduce(expected_ty, v)
                                }
                            } else {
                                v // Can't coerce, use as-is
                            };
                            args.push(BlockArg::Value(coerced));
                        } else {
                            // Value not yet translated — use zero fallback with correct type
                            let zero = if expected_ty.is_int() {
                                self.builder.ins().iconst(expected_ty, 0)
                            } else if expected_ty == types::F32 {
                                self.builder.ins().f32const(0.0)
                            } else if expected_ty == types::F64 {
                                self.builder.ins().f64const(0.0)
                            } else {
                                self.builder.ins().iconst(types::I64, 0)
                            };
                            args.push(BlockArg::Value(zero));
                        }
                        found = true;
                        break;
                    }
                }
                if !found {
                    let zero = if expected_ty.is_int() {
                        self.builder.ins().iconst(expected_ty, 0)
                    } else {
                        self.builder.ins().iconst(types::I64, 0)
                    };
                    args.push(BlockArg::Value(zero));
                }
            }
        }
        Ok(args)
    }

    // ========================================================================
    // Tier 2: Aggregate instructions
    // ========================================================================

    fn translate_struct_init(
        &mut self,
        struct_name: &str,
        fields: &[Value],
    ) -> BridgeResult<ClifValue> {
        let field_defs = self.struct_defs.get(struct_name).cloned();
        let total_size = if let Some(ref fdefs) = field_defs {
            let field_types: Vec<&MirType> = fdefs.iter().map(|f| &f.ty).collect();
            let (_, size) = ty::compute_struct_layout(&field_types);
            size
        } else {
            (fields.len() as u32) * 8
        };

        let slot = self.builder.create_sized_stack_slot(make_stack_slot(total_size.max(8)));
        let base_addr = self.builder.ins().stack_addr(POINTER_TYPE, slot, 0);

        if let Some(ref fdefs) = field_defs {
            let field_types: Vec<&MirType> = fdefs.iter().map(|f| &f.ty).collect();
            let (offsets, _) = ty::compute_struct_layout(&field_types);
            for (i, field_val) in fields.iter().enumerate() {
                if i < offsets.len() {
                    let v = self.get_value(field_val)?;
                    self.builder
                        .ins()
                        .store(MemFlags::new(), v, base_addr, offsets[i] as i32);
                }
            }
        } else {
            for (i, field_val) in fields.iter().enumerate() {
                let v = self.get_value(field_val)?;
                self.builder
                    .ins()
                    .store(MemFlags::new(), v, base_addr, (i * 8) as i32);
            }
        }

        Ok(base_addr)
    }

    fn translate_enum_init(
        &mut self,
        enum_name: &str,
        variant_name: &str,
        payload: &[Value],
    ) -> BridgeResult<ClifValue> {
        let variant_idx = if let Some(edef) = self.enum_defs.get(enum_name) {
            edef.iter()
                .position(|v| v.name == variant_name)
                .unwrap_or(0)
        } else {
            0
        };

        let payload_size = (payload.len() as u32) * 8;
        let total_size = (8 + payload_size).max(8);

        let slot = self.builder.create_sized_stack_slot(make_stack_slot(total_size));
        let base_addr = self.builder.ins().stack_addr(POINTER_TYPE, slot, 0);

        let tag_val = self.builder.ins().iconst(types::I64, variant_idx as i64);
        self.builder
            .ins()
            .store(MemFlags::new(), tag_val, base_addr, 0);

        for (i, pval) in payload.iter().enumerate() {
            let v = self.get_value(pval)?;
            self.builder
                .ins()
                .store(MemFlags::new(), v, base_addr, (8 + i * 8) as i32);
        }

        Ok(base_addr)
    }

    fn translate_tuple_init(&mut self, elements: &[Value]) -> BridgeResult<ClifValue> {
        let total_size = ((elements.len() as u32) * 8).max(8);
        let slot = self.builder.create_sized_stack_slot(make_stack_slot(total_size));
        let base_addr = self.builder.ins().stack_addr(POINTER_TYPE, slot, 0);

        for (i, elem) in elements.iter().enumerate() {
            let v = self.get_value(elem)?;
            self.builder
                .ins()
                .store(MemFlags::new(), v, base_addr, (i * 8) as i32);
        }

        Ok(base_addr)
    }

    fn translate_array_init(
        &mut self,
        element_type: &MirType,
        elements: &[Value],
    ) -> BridgeResult<ClifValue> {
        let elem_size = ty::type_size(element_type);
        let total_size = (elem_size * elements.len() as u32).max(8);

        let slot = self.builder.create_sized_stack_slot(make_stack_slot(total_size));
        let base_addr = self.builder.ins().stack_addr(POINTER_TYPE, slot, 0);

        for (i, elem) in elements.iter().enumerate() {
            let v = self.get_value(elem)?;
            let offset = (i as u32) * elem_size;
            self.builder
                .ins()
                .store(MemFlags::new(), v, base_addr, offset as i32);
        }

        Ok(base_addr)
    }

    fn translate_gep(
        &mut self,
        base: &Value,
        indices: &[Value],
    ) -> BridgeResult<ClifValue> {
        let mut addr = self.get_value(base)?;
        // Ensure base address is pointer-sized
        let addr_ty = self.builder.func.dfg.value_type(addr);
        if addr_ty != POINTER_TYPE && addr_ty.is_int() {
            addr = if addr_ty.bytes() < POINTER_TYPE.bytes() {
                self.builder.ins().uextend(POINTER_TYPE, addr)
            } else {
                self.builder.ins().ireduce(POINTER_TYPE, addr)
            };
        }

        for idx in indices {
            let mut idx_val = self.get_value(idx)?;
            // Coerce index to pointer-sized integer for arithmetic
            let idx_ty = self.builder.func.dfg.value_type(idx_val);
            if idx_ty != POINTER_TYPE && idx_ty.is_int() {
                idx_val = if idx_ty.bytes() < POINTER_TYPE.bytes() {
                    self.builder.ins().sextend(POINTER_TYPE, idx_val)
                } else {
                    self.builder.ins().ireduce(POINTER_TYPE, idx_val)
                };
            }
            let eight = self.builder.ins().iconst(POINTER_TYPE, 8);
            let offset = self.builder.ins().imul(idx_val, eight);
            addr = self.builder.ins().iadd(addr, offset);
        }

        Ok(addr)
    }

    fn translate_extract_value(
        &mut self,
        aggregate: &Value,
        indices: &[u32],
    ) -> BridgeResult<ClifValue> {
        let base = self.get_value(aggregate)?;

        let mut offset: u32 = 0;
        for &idx in indices {
            offset += idx * 8;
        }

        let val = self
            .builder
            .ins()
            .load(types::I64, MemFlags::new(), base, offset as i32);
        Ok(val)
    }

    fn translate_insert_value(
        &mut self,
        aggregate: &Value,
        value: &Value,
        indices: &[u32],
    ) -> BridgeResult<ClifValue> {
        let base = self.get_value(aggregate)?;
        let val = self.get_value(value)?;

        let mut offset: u32 = 0;
        for &idx in indices {
            offset += idx * 8;
        }

        self.builder
            .ins()
            .store(MemFlags::new(), val, base, offset as i32);

        Ok(base)
    }

    fn translate_closure_init(
        &mut self,
        func_name: &str,
        captures: &[(String, Value)],
    ) -> BridgeResult<ClifValue> {
        let total_size = ((1 + captures.len()) as u32 * 8).max(8);

        let slot = self.builder.create_sized_stack_slot(make_stack_slot(total_size));
        let base_addr = self.builder.ins().stack_addr(POINTER_TYPE, slot, 0);

        // Look up function ID by MIR name, or try with tml_ prefix
        let func_id_opt = self.func_ids.get(func_name).copied()
            .or_else(|| {
                let sym = self.resolve_symbol_name(func_name);
                self.func_ids.get(&sym).copied()
            });
        if let Some(func_id) = func_id_opt {
            let local_fn = self.module.declare_func_in_func(func_id, self.builder.func);
            let fn_ptr = self.builder.ins().func_addr(POINTER_TYPE, local_fn);
            self.builder
                .ins()
                .store(MemFlags::new(), fn_ptr, base_addr, 0);
        } else {
            let null = self.builder.ins().iconst(POINTER_TYPE, 0);
            self.builder
                .ins()
                .store(MemFlags::new(), null, base_addr, 0);
        }

        for (i, (_, cap_val)) in captures.iter().enumerate() {
            let v = self.get_value(cap_val)?;
            self.builder
                .ins()
                .store(MemFlags::new(), v, base_addr, ((i + 1) * 8) as i32);
        }

        Ok(base_addr)
    }
}

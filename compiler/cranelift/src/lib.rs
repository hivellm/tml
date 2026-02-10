/// TML Cranelift Bridge — C API Entry Points
///
/// This crate provides a C-compatible FFI layer for the Cranelift code generator.
/// The C++ compiler serializes MIR to binary, calls these functions, and receives
/// object file bytes or IR text back.

mod error;
mod mir_reader;
mod mir_types;
mod translate;
mod types;

use std::ffi::{CStr, CString};
use std::panic;
use std::ptr;
use std::slice;

use error::BridgeResult;
use mir_reader::MirBinaryReader;
use translate::ModuleTranslator;

/// Result struct returned to C++.
#[repr(C)]
pub struct CraneliftResult {
    pub success: i32,
    pub data: *const u8,
    pub data_len: usize,
    pub ir_text: *const i8,
    pub ir_text_len: usize,
    pub error_msg: *const i8,
}

/// Options struct received from C++.
#[repr(C)]
pub struct CraneliftOptions {
    pub optimization_level: i32,
    pub target_triple: *const i8,
    pub debug_info: i32,
    pub dll_export: i32,
}

impl CraneliftResult {
    fn success_with_data(data: Vec<u8>) -> Self {
        let len = data.len();
        let ptr = data.as_ptr();
        std::mem::forget(data); // C++ will call cranelift_free_result
        Self {
            success: 1,
            data: ptr,
            data_len: len,
            ir_text: ptr::null(),
            ir_text_len: 0,
            error_msg: ptr::null(),
        }
    }

    fn success_with_ir(ir: String) -> Self {
        let cstr = CString::new(ir).unwrap_or_default();
        let len = cstr.as_bytes().len();
        let ptr = cstr.as_ptr();
        std::mem::forget(cstr);
        Self {
            success: 1,
            data: ptr::null(),
            data_len: 0,
            ir_text: ptr,
            ir_text_len: len,
            error_msg: ptr::null(),
        }
    }

    fn error(msg: String) -> Self {
        let cstr = CString::new(msg).unwrap_or_default();
        let ptr = cstr.as_ptr();
        std::mem::forget(cstr);
        Self {
            success: 0,
            data: ptr::null(),
            data_len: 0,
            ir_text: ptr::null(),
            ir_text_len: 0,
            error_msg: ptr,
        }
    }
}

fn get_target_triple(opts: &CraneliftOptions) -> String {
    if opts.target_triple.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(opts.target_triple) }
        .to_str()
        .unwrap_or("")
        .to_string()
}

fn compile_mir_impl(
    mir_data: &[u8],
    func_indices: Option<&[usize]>,
    opts: &CraneliftOptions,
) -> BridgeResult<Vec<u8>> {
    let mut reader = MirBinaryReader::new(mir_data);
    let module = reader.read_module()?;

    let target = get_target_triple(opts);
    let opt_level = opts.optimization_level.max(0).min(3) as u8;

    let mut translator = ModuleTranslator::new(&target, opt_level)?;
    translator.translate_module(&module, func_indices)?;
    translator.finish()
}

fn generate_ir_impl(mir_data: &[u8], opts: &CraneliftOptions) -> BridgeResult<String> {
    let mut reader = MirBinaryReader::new(mir_data);
    let module = reader.read_module()?;

    let target = get_target_triple(opts);
    let opt_level = opts.optimization_level.max(0).min(3) as u8;

    let mut translator = ModuleTranslator::new(&target, opt_level)?;
    translator.generate_ir_text(&module)
}

/// Catch panics and convert to CraneliftResult.
fn catch_and_convert<F: FnOnce() -> CraneliftResult + panic::UnwindSafe>(f: F) -> CraneliftResult {
    match panic::catch_unwind(f) {
        Ok(result) => result,
        Err(e) => {
            let msg = if let Some(s) = e.downcast_ref::<&str>() {
                s.to_string()
            } else if let Some(s) = e.downcast_ref::<String>() {
                s.clone()
            } else {
                "unknown panic in Cranelift bridge".to_string()
            };
            CraneliftResult::error(format!("PANIC: {}", msg))
        }
    }
}

// ============================================================================
// C API
// ============================================================================

/// Compile a full MIR module to an object file.
#[unsafe(no_mangle)]
pub extern "C" fn cranelift_compile_mir(
    mir_data: *const u8,
    mir_len: usize,
    options: *const CraneliftOptions,
) -> CraneliftResult {
    catch_and_convert(move || {
        if mir_data.is_null() || mir_len == 0 {
            return CraneliftResult::error("null or empty MIR data".into());
        }
        let data = unsafe { slice::from_raw_parts(mir_data, mir_len) };
        let opts = if options.is_null() {
            CraneliftOptions {
                optimization_level: 0,
                target_triple: ptr::null(),
                debug_info: 0,
                dll_export: 0,
            }
        } else {
            unsafe { ptr::read(options) }
        };

        match compile_mir_impl(data, None, &opts) {
            Ok(obj_bytes) => CraneliftResult::success_with_data(obj_bytes),
            Err(e) => CraneliftResult::error(e.to_string()),
        }
    })
}

/// Compile a subset of functions from a MIR module (CGU mode).
#[unsafe(no_mangle)]
pub extern "C" fn cranelift_compile_mir_cgu(
    mir_data: *const u8,
    mir_len: usize,
    func_indices: *const usize,
    num_indices: usize,
    options: *const CraneliftOptions,
) -> CraneliftResult {
    catch_and_convert(move || {
        if mir_data.is_null() || mir_len == 0 {
            return CraneliftResult::error("null or empty MIR data".into());
        }
        let data = unsafe { slice::from_raw_parts(mir_data, mir_len) };
        let indices = if func_indices.is_null() || num_indices == 0 {
            None
        } else {
            Some(unsafe { slice::from_raw_parts(func_indices, num_indices) })
        };
        let opts = if options.is_null() {
            CraneliftOptions {
                optimization_level: 0,
                target_triple: ptr::null(),
                debug_info: 0,
                dll_export: 0,
            }
        } else {
            unsafe { ptr::read(options) }
        };

        match compile_mir_impl(data, indices, &opts) {
            Ok(obj_bytes) => CraneliftResult::success_with_data(obj_bytes),
            Err(e) => CraneliftResult::error(e.to_string()),
        }
    })
}

/// Generate Cranelift IR text from a MIR module (no compilation).
#[unsafe(no_mangle)]
pub extern "C" fn cranelift_generate_ir(
    mir_data: *const u8,
    mir_len: usize,
    options: *const CraneliftOptions,
) -> CraneliftResult {
    catch_and_convert(move || {
        if mir_data.is_null() || mir_len == 0 {
            return CraneliftResult::error("null or empty MIR data".into());
        }
        let data = unsafe { slice::from_raw_parts(mir_data, mir_len) };
        let opts = if options.is_null() {
            CraneliftOptions {
                optimization_level: 0,
                target_triple: ptr::null(),
                debug_info: 0,
                dll_export: 0,
            }
        } else {
            unsafe { ptr::read(options) }
        };

        match generate_ir_impl(data, &opts) {
            Ok(ir_text) => CraneliftResult::success_with_ir(ir_text),
            Err(e) => CraneliftResult::error(e.to_string()),
        }
    })
}

/// Free a CraneliftResult. Must be called for every result returned.
#[unsafe(no_mangle)]
pub extern "C" fn cranelift_free_result(result: *mut CraneliftResult) {
    if result.is_null() {
        return;
    }
    let r = unsafe { &*result };

    if !r.data.is_null() && r.data_len > 0 {
        unsafe {
            let _ = Vec::from_raw_parts(r.data as *mut u8, r.data_len, r.data_len);
        }
    }
    if !r.ir_text.is_null() && r.ir_text_len > 0 {
        unsafe {
            let _ = CString::from_raw(r.ir_text as *mut i8);
        }
    }
    if !r.error_msg.is_null() {
        unsafe {
            let _ = CString::from_raw(r.error_msg as *mut i8);
        }
    }

    // Zero out the struct so C++ doesn't double-free
    unsafe {
        ptr::write_bytes(result, 0, 1);
    }
}

/// Get the Cranelift version string.
#[unsafe(no_mangle)]
pub extern "C" fn cranelift_version() -> *const i8 {
    // Return a static string
    static VERSION: &[u8] = b"cranelift-0.128\0";
    VERSION.as_ptr() as *const i8
}

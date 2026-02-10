use std::fmt;

#[derive(Debug)]
pub enum BridgeError {
    MirDeserialize(String),
    Translation(String),
    Codegen(String),
    UnsupportedInstruction(String),
    InvalidTarget(String),
}

impl fmt::Display for BridgeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            BridgeError::MirDeserialize(msg) => write!(f, "MIR deserialization error: {}", msg),
            BridgeError::Translation(msg) => write!(f, "translation error: {}", msg),
            BridgeError::Codegen(msg) => write!(f, "codegen error: {}", msg),
            BridgeError::UnsupportedInstruction(msg) => {
                write!(f, "unsupported instruction: {}", msg)
            }
            BridgeError::InvalidTarget(msg) => write!(f, "invalid target: {}", msg),
        }
    }
}

impl std::error::Error for BridgeError {}

pub type BridgeResult<T> = Result<T, BridgeError>;

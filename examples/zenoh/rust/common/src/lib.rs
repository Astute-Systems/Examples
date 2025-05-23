include!(concat!(env!("OUT_DIR"), "/protos/mod.rs"));
include!(concat!(env!("OUT_DIR"), "/version.rs"));
pub mod utils;
pub mod zenoh;

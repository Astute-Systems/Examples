// Copyright (c) 2025,Astute Systems PTY LTD
//
// This file is part of the HORAS project developed byAstute Systems.
//
// See the commercial LICENSE file in the project directory for full license details.
//
//! Build script for the common crate.
use std::io::Write;

///
/// Use the version file in the root to create a version.rs file
///
/// File contains MAJOR, MINOR, PATCH and SUFFIX in the following format:
/// ```text
/// 1
/// 2
/// 3
/// alpha1    
/// ```
///
fn version() {
    // File is in project root
    let version_file = std::env::var("CARGO_MANIFEST_DIR").unwrap() + "/../../../../version";

    // Check if the file exists
    if !std::path::Path::new(&version_file).exists() {
        panic!("Version file not found: {}", version_file);
    }

    // Read the version file
    let version_content =
        std::fs::read_to_string(&version_file).expect("Failed to read version file");

    // Split the file into lines
    let version_lines: Vec<&str> = version_content.lines().map(|s| s.trim()).collect();

    // Ensure the file has exactly 4 lines
    if version_lines.len() < 4 {
        panic!(
            "Version file must have exactly 4 lines: MAJOR, MINOR, PATCH, SUFFIX. Found {} lines.",
            version_lines.len()
        );
    }

    // Extract version components
    let major = &version_lines[0];
    let minor = &version_lines[1];
    let patch = &version_lines[2];
    let suffix = &version_lines[3];

    // Construct the full version string
    let version = format!("{}.{}.{}-{}", major, minor, patch, suffix);
    println!("cargo:rustc-env=VERSION={}", version);

    // Create version.rs in the staging area
    let version_file = std::env::var("OUT_DIR").unwrap() + "/version.rs";
    let mut file = std::fs::File::create(&version_file).expect("Failed to create version.rs file");

    // Write constants to version.rs
    writeln!(file, "pub const MAJOR: &str = \"{}\";", major).unwrap();
    writeln!(file, "pub const MINOR: &str = \"{}\";", minor).unwrap();
    writeln!(file, "pub const PATCH: &str = \"{}\";", patch).unwrap();
    writeln!(file, "pub const SUFFIX: &str = \"{}\";", suffix).unwrap();
    writeln!(file, "pub const VERSION_STRING: &str = \"{}\";", version).unwrap();
    writeln!(file, "").unwrap();

    // Create fn
    writeln!(file, "pub fn print_version(version: &str) {{").unwrap();
    writeln!(file, "    println!(\"Version: {{}}\", version);").unwrap();
    writeln!(
        file,
        "    println!(\"System Version: {{}}\", VERSION_STRING);"
    )
    .unwrap();
    writeln!(file, "}}").unwrap();

    // Print the path to the generated file
    println!("cargo:rerun-if-changed={}", version_file);
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=path/to/Cargo.lock");
    protobuf_codegen::Codegen::new()
        .cargo_out_dir("protos")
        .include("../../../../icds/proto")
        .input("../../../../icds/proto/common_can.proto")
        .run_from_script();
    version();
}

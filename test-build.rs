use glob::glob;

fn main() {
    let cargo_home = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    panic!();
    const PATH_LIB: &str =
        "/home/lemolatoon/workspace/mikanos-build/devenv_src/stdlib/target/x86_64-elf/lib";
    const PATH_INCLUDE: &str =
        "/home/lemolatoon/workspace/mikanos-build/devenv_src/stdlib/target/x86_64-elf/include";
    const PATH_INCLUDE_CPP: &str =
        "/home/lemolatoon/workspace/mikanos-build/devenv_src/stdlib/target/x86_64-elf/include/c++/v1";
    let cxx_flags: String = format!(
        "-Wall -g -ffreestanding -mno-red-zone -fno-exceptions -std=c++17 -fpermissive -fno-rtti -L {} -I {} -I {} -I {} -I {}/external -w",
        PATH_LIB, PATH_INCLUDE_CPP, PATH_INCLUDE, &cargo_home, &cargo_home
    );
    let mut cc = cxx_build::bridges(vec!["src/lib.rs", "tests/error.rs"]);
    for flag in cxx_flags.split_whitespace() {
        cc.flag(flag);
    }
    let file_iter = glob("usb/**/*.cpp")
        .unwrap()
        .chain(glob("external/**/*.cpp").unwrap());

    let file_iter = file_iter.chain(glob("test_cpp/**/*.cpp").unwrap());
    let file_iter = file_iter.map(|e| e.unwrap());
    cc.files(file_iter).compile("usb");
}

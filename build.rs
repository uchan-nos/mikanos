use glob::glob;

fn main() {
    let cargo_home = std::env::var("CARGO_MANIFEST_DIR").unwrap();
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
    let mut cc = cxx_build::bridge("src/lib.rs");
    for flag in cxx_flags.split_whitespace() {
        cc.flag(flag);
    }
    cc.files(
        glob("usb/**/*.cpp")
            .unwrap()
            .chain(glob("external/**/*.cpp").unwrap())
            .map(|e| e.unwrap()),
    )
    .compile("usb");
}

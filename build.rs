use glob::glob;

fn main() {
    let cargo_home = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let cxx_flags: String = format!(
        "-Wall -g -ffreestanding -mno-red-zone -fno-exceptions -std=c++17 -fpermissive -fno-rtti -I {} -I {}/external",
        &cargo_home, &cargo_home
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

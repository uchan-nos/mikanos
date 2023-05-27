use core::sync::atomic::AtomicUsize;
use cxx::private::c_char;

#[cxx::bridge]
pub mod ffi {
    unsafe extern "C++" {
        include!("mikanos-usb/external/error.hpp");
        type Error;
        fn Name(self: &Error) -> *const c_char;
        fn File(self: &Error) -> *const c_char;
        fn Line(self: &Error) -> i32;
    }
    unsafe extern "C++" {
        include!("test_cpp/test.hpp");
        fn new_success() -> UniquePtr<Error>;
    }

    #[namespace = "usb::xhci"]
    unsafe extern "C++" {
        include!("mikanos-usb/usb/xhci/xhci.hpp");
        type Controller;
        fn Initialize(self: Pin<&mut Controller>) -> UniquePtr<Error>;
    }
}

#[cxx::bridge(namespace = "rust")]
mod rust {
    extern "Rust" {
        unsafe fn put_string(s: *const c_char) -> bool;
    }
}

type PrintFunc = fn(&str);
static LOG_PRINTER: AtomicUsize = AtomicUsize::new(0);

pub fn set_log_printer(printer: PrintFunc) {
    LOG_PRINTER.store(printer as usize, core::sync::atomic::Ordering::Release);
}

#[doc(hidden)]
unsafe fn put_string(s: *const c_char) -> bool {
    let Ok(s) = unsafe { core::ffi::CStr::from_ptr(s) }.to_str() else {return false};
    let printer = LOG_PRINTER.load(core::sync::atomic::Ordering::Acquire);
    if printer != 0 {
        let f: PrintFunc = unsafe { core::mem::transmute(printer) };
        f(s);
        true
    } else {
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn error_test() {
        use core::ffi::CStr;
        let error = ffi::new_success();
        let name = unsafe { CStr::from_ptr(error.Name()) }.to_str().unwrap();
        let file = unsafe { CStr::from_ptr(error.File()) }.to_str().unwrap();
        let line = error.Line();
        assert_eq!(name, "kSuccess");
        assert_eq!(file, "test_cpp/test.cpp");
        assert_eq!(line, 6);
    }
}

#![no_std]

use core::sync::atomic::AtomicUsize;
use cxx::private::c_char;

#[cxx::bridge]
mod cpp_error {
    unsafe extern "C++" {
        include!("mikanos-usb/external/error.hpp");
        type Error;
        fn Cause(&self) -> i32;
        fn Name(&self) -> *const c_char;
        fn File(&self) -> *const c_char;
        fn Line(&self) -> i32;
    }
}

#[cxx::bridge(namespace = "usb::xhci")]
mod xhci {
    unsafe extern "C++" {
        include!("mikanos-usb/usb/xhci/xhci.hpp");
        type Controller;
        type Error = crate::cpp_error::Error;
        fn initialize(self: &Controller) -> Error;
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

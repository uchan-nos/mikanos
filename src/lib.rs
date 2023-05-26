pub mod asm;
use cxx::{bridge, private::c_char};

#[cxx::bridge(namespace = "usb::xhci")]
mod xhci {}

#[cxx::bridge(namespace = "rust")]
mod rust {
    extern "Rust" {
        unsafe fn put_string(s: *const c_char) -> bool;
    }
}

extern "C" {
    fn IoOut32(addr: u16, data: u32);
    fn IoIn32(addr: u16) -> u32;
}

pub trait LogPrinter {
    fn print(&mut self, s: &str);

    fn print_cstr(&mut self, s: &core::ffi::CStr) {
        self.print(s.to_str().unwrap());
    }
}

static LOG_PRINTER: core::sync::atomic::AtomicPtr<dyn LogPrinter> =
    core::sync::atomic::AtomicPtr::new(core::ptr::null_mut());

pub fn set_log_printer(printer: &'static mut dyn LogPrinter) {
    LOG_PRINTER.store(printer.as_ptr(), core::sync::atomic::Ordering::Relaxed);
}

#[doc(hidden)]
unsafe fn put_string(s: *const c_char) -> bool {
    let s = unsafe { core::ffi::CStr::from_ptr(s) };
    let printer = LOG_PRINTER.load(core::sync::atomic::Ordering::Relaxed);
    if !printer.is_null() {
        unsafe { (*printer).print_cstr(s) };
        true
    } else {
        false
    }
}

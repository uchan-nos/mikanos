#![feature(prelude_import)]
#![no_std]
#[prelude_import]
use core::prelude::rust_2021::*;
#[macro_use]
extern crate core;
#[macro_use]
extern crate compiler_builtins;
use core::sync::atomic::AtomicUsize;
use cxx::private::c_char;
#[deny(improper_ctypes, improper_ctypes_definitions)]
#[allow(clippy::unknown_clippy_lints)]
#[allow(
    non_camel_case_types,
    non_snake_case,
    clippy::extra_unused_type_parameters,
    clippy::ptr_as_ptr,
    clippy::upper_case_acronyms,
    clippy::use_self
)]
mod cpp_error {
    #[repr(C)]
    pub struct Error {
        _private: ::cxx::private::Opaque,
    }
    unsafe impl ::cxx::ExternType for Error {
        #[allow(unused_attributes)]
        #[doc(hidden)]
        type Id = (::cxx::E, ::cxx::r, ::cxx::r, ::cxx::o, ::cxx::r);
        type Kind = ::cxx::kind::Opaque;
    }
    impl Error {
        pub fn Cause(&self) -> i32 {
            extern "C" {
                #[link_name = "cxxbridge1$Error$Cause"]
                fn __Cause(_: &Error) -> i32;
            }
            unsafe { __Cause(self) }
        }
    }
    impl Error {
        pub fn Name(&self) -> *const ::cxx::private::c_char {
            extern "C" {
                #[link_name = "cxxbridge1$Error$Name"]
                fn __Name(_: &Error) -> *const ::cxx::private::c_char;
            }
            unsafe { __Name(self) }
        }
    }
    impl Error {
        pub fn File(&self) -> *const ::cxx::private::c_char {
            extern "C" {
                #[link_name = "cxxbridge1$Error$File"]
                fn __File(_: &Error) -> *const ::cxx::private::c_char;
            }
            unsafe { __File(self) }
        }
    }
    impl Error {
        pub fn Line(&self) -> i32 {
            extern "C" {
                #[link_name = "cxxbridge1$Error$Line"]
                fn __Line(_: &Error) -> i32;
            }
            unsafe { __Line(self) }
        }
    }
    #[doc(hidden)]
    const _: () = {
        let _: fn() = {
            trait __AmbiguousIfImpl<A> {
                fn infer() {}
            }
            impl<T> __AmbiguousIfImpl<()> for T where T: ?::cxx::core::marker::Sized {}
            #[allow(dead_code)]
            struct __Invalid;
            impl<T> __AmbiguousIfImpl<__Invalid> for T where
                T: ?::cxx::core::marker::Sized + ::cxx::core::marker::Unpin
            {
            }
            <Error as __AmbiguousIfImpl<_>>::infer
        };
    };
}
#[deny(improper_ctypes, improper_ctypes_definitions)]
#[allow(clippy::unknown_clippy_lints)]
#[allow(
    non_camel_case_types,
    non_snake_case,
    clippy::extra_unused_type_parameters,
    clippy::ptr_as_ptr,
    clippy::upper_case_acronyms,
    clippy::use_self
)]
mod xhci {
    #[repr(C)]
    pub struct Controller {
        _private: ::cxx::private::Opaque,
    }
    unsafe impl ::cxx::ExternType for Controller {
        #[allow(unused_attributes)]
        #[doc(hidden)]
        type Id = (
            ::cxx::u,
            ::cxx::s,
            ::cxx::b,
            (),
            ::cxx::x,
            ::cxx::h,
            ::cxx::c,
            ::cxx::i,
            (),
            ::cxx::C,
            ::cxx::o,
            ::cxx::n,
            ::cxx::t,
            ::cxx::r,
            ::cxx::o,
            ::cxx::l,
            ::cxx::l,
            ::cxx::e,
            ::cxx::r,
        );
        type Kind = ::cxx::kind::Opaque;
    }
    pub type Error = crate::cpp_error::Error;
    impl Controller {
        pub fn initialize(&self) -> Error {
            extern "C" {
                #[link_name = "usb$xhci$cxxbridge1$Controller$initialize"]
                fn __initialize(_: &Controller, __return: *mut Error);
            }
            unsafe {
                let mut __return = ::cxx::core::mem::MaybeUninit::<Error>::uninit();
                __initialize(self, __return.as_mut_ptr());
                __return.assume_init()
            }
        }
    }
    #[doc(hidden)]
    const _: () = {
        let _: fn() = {
            trait __AmbiguousIfImpl<A> {
                fn infer() {}
            }
            impl<T> __AmbiguousIfImpl<()> for T where T: ?::cxx::core::marker::Sized {}
            #[allow(dead_code)]
            struct __Invalid;
            impl<T> __AmbiguousIfImpl<__Invalid> for T where
                T: ?::cxx::core::marker::Sized + ::cxx::core::marker::Unpin
            {
            }
            <Controller as __AmbiguousIfImpl<_>>::infer
        };
        const _: fn() = ::cxx::private::verify_extern_type::<
            Error,
            (
                ::cxx::u,
                ::cxx::s,
                ::cxx::b,
                (),
                ::cxx::x,
                ::cxx::h,
                ::cxx::c,
                ::cxx::i,
                (),
                ::cxx::E,
                ::cxx::r,
                ::cxx::r,
                ::cxx::o,
                ::cxx::r,
            ),
        >;
        const _: fn() = ::cxx::private::verify_extern_kind::<Error, ::cxx::kind::Trivial>;
    };
}
#[deny(improper_ctypes, improper_ctypes_definitions)]
#[allow(clippy::unknown_clippy_lints)]
#[allow(
    non_camel_case_types,
    non_snake_case,
    clippy::extra_unused_type_parameters,
    clippy::ptr_as_ptr,
    clippy::upper_case_acronyms,
    clippy::use_self
)]
mod rust {
    #[doc(hidden)]
    const _: () = {
        #[doc(hidden)]
        #[export_name = "rust$cxxbridge1$put_string"]
        unsafe extern "C" fn __put_string(s: *const ::cxx::private::c_char) -> bool {
            let __fn = "mikanos_usb::rust::put_string";
            unsafe fn __put_string(s: *const ::cxx::private::c_char) -> bool {
                super::put_string(s)
            }
            ::cxx::private::prevent_unwind(__fn, move || __put_string(s))
        }
    };
}
type PrintFunc = fn(&str);
static LOG_PRINTER: AtomicUsize = AtomicUsize::new(0);
pub fn set_log_printer(printer: PrintFunc) {
    LOG_PRINTER.store(printer as usize, core::sync::atomic::Ordering::Release);
}
#[doc(hidden)]
unsafe fn put_string(s: *const c_char) -> bool {
    let Ok (s) = unsafe { core :: ffi :: CStr :: from_ptr (s) } . to_str () else { return false } ;
    let printer = LOG_PRINTER.load(core::sync::atomic::Ordering::Acquire);
    if printer != 0 {
        let f: PrintFunc = unsafe { core::mem::transmute(printer) };
        f(s);
        true
    } else {
        false
    }
}

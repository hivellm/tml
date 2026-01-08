//! # API Stability Tracking
//!
//! This module defines stability levels for TML APIs. Stability attributes
//! help manage API evolution and communicate expectations to users.
//!
//! ## Usage
//!
//! Functions and types can be annotated with stability decorators:
//!
//! ```tml
//! @stable
//! pub func core_function() { }
//!
//! @deprecated("Use new_api() instead")
//! pub func old_function() { }
//! ```

#ifndef TML_TYPES_ENV_STABILITY_HPP
#define TML_TYPES_ENV_STABILITY_HPP

namespace tml::types {

/// API stability level for evolution management.
///
/// Used with `@stable`, `@unstable`, and `@deprecated` decorators to
/// communicate API stability guarantees to users.
enum class StabilityLevel {
    Stable,    ///< `@stable` - API is stable and won't change.
    Unstable,  ///< Default - API may change in future versions.
    Deprecated ///< `@deprecated` - API will be removed in future versions.
};

} // namespace tml::types

#endif // TML_TYPES_ENV_STABILITY_HPP

#ifndef TML_TYPES_ENV_STABILITY_HPP
#define TML_TYPES_ENV_STABILITY_HPP

namespace tml::types {

// Stability level for API evolution management
enum class StabilityLevel {
    Stable,    // @stable - API is stable and won't change
    Unstable,  // No annotation - API may change in future versions
    Deprecated // @deprecated - API will be removed in future versions
};

} // namespace tml::types

#endif // TML_TYPES_ENV_STABILITY_HPP

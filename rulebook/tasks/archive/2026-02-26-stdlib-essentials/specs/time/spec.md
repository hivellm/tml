# Spec: Time Module

## Overview

Time measurement and calendar types.

## Types

### Duration

Time span with nanosecond precision.

```tml
pub type Duration {
    secs: U64,
    nanos: U32,  // 0..999_999_999
}

extend Duration {
    pub const ZERO: Duration = Duration { secs: 0, nanos: 0 }
    pub const MAX: Duration = Duration { secs: U64::MAX, nanos: 999_999_999 }
    pub const SECOND: Duration = Duration { secs: 1, nanos: 0 }
    pub const MILLISECOND: Duration = Duration { secs: 0, nanos: 1_000_000 }
    pub const MICROSECOND: Duration = Duration { secs: 0, nanos: 1_000 }
    pub const NANOSECOND: Duration = Duration { secs: 0, nanos: 1 }

    pub func from_secs(secs: U64) -> Duration
    pub func from_millis(millis: U64) -> Duration
    pub func from_micros(micros: U64) -> Duration
    pub func from_nanos(nanos: U64) -> Duration
    pub func from_secs_f64(secs: F64) -> Duration

    pub func as_secs(this) -> U64
    pub func as_millis(this) -> U128
    pub func as_micros(this) -> U128
    pub func as_nanos(this) -> U128
    pub func as_secs_f64(this) -> F64
    pub func as_millis_f64(this) -> F64
    pub func subsec_millis(this) -> U32
    pub func subsec_micros(this) -> U32
    pub func subsec_nanos(this) -> U32

    pub func is_zero(this) -> Bool

    pub func checked_add(this, rhs: Duration) -> Maybe[Duration]
    pub func checked_sub(this, rhs: Duration) -> Maybe[Duration]
    pub func checked_mul(this, rhs: U32) -> Maybe[Duration]
    pub func checked_div(this, rhs: U32) -> Maybe[Duration]

    pub func saturating_add(this, rhs: Duration) -> Duration
    pub func saturating_sub(this, rhs: Duration) -> Duration
    pub func saturating_mul(this, rhs: U32) -> Duration
}

extend Duration with Add {
    type Output = Duration
    func add(this, rhs: Duration) -> Duration
}

extend Duration with Sub {
    type Output = Duration
    func sub(this, rhs: Duration) -> Duration
}
```

### Instant

Monotonic clock for measuring elapsed time.

```tml
pub type Instant {
    inner: U64,  // Platform-specific representation
}

extend Instant {
    /// Capture current instant
    pub func now() -> Instant

    /// Time elapsed since this instant
    pub func elapsed(this) -> Duration

    /// Duration since earlier instant
    pub func duration_since(this, earlier: Instant) -> Duration

    /// Checked duration since (returns Nothing if earlier is later)
    pub func checked_duration_since(this, earlier: Instant) -> Maybe[Duration]

    /// Saturating duration since
    pub func saturating_duration_since(this, earlier: Instant) -> Duration

    pub func checked_add(this, duration: Duration) -> Maybe[Instant]
    pub func checked_sub(this, duration: Duration) -> Maybe[Instant]
}

extend Instant with Add[Duration] {
    type Output = Instant
}

extend Instant with Sub[Duration] {
    type Output = Instant
}

extend Instant with Sub[Instant] {
    type Output = Duration
}
```

### SystemTime

Wall-clock time (can go backwards).

```tml
pub type SystemTime {
    inner: I128,  // Nanoseconds since UNIX_EPOCH (can be negative)
}

extend SystemTime {
    pub const UNIX_EPOCH: SystemTime = SystemTime { inner: 0 }

    /// Get current system time
    pub func now() -> SystemTime

    /// Duration since earlier time
    pub func duration_since(this, earlier: SystemTime) -> Outcome[Duration, SystemTimeError]

    /// Time elapsed since this time
    pub func elapsed(this) -> Outcome[Duration, SystemTimeError]

    pub func checked_add(this, duration: Duration) -> Maybe[SystemTime]
    pub func checked_sub(this, duration: Duration) -> Maybe[SystemTime]
}

pub type SystemTimeError {
    duration: Duration,  // The "negative" duration
}
```

### DateTime

High-level calendar type.

```tml
pub type DateTime {
    year: I32,
    month: U8,    // 1-12
    day: U8,      // 1-31
    hour: U8,     // 0-23
    minute: U8,   // 0-59
    second: U8,   // 0-59
    nanos: U32,   // 0-999_999_999
    offset: I16,  // Minutes from UTC (-720 to +840)
}

extend DateTime {
    /// Current local time
    pub func now() -> DateTime

    /// Current UTC time
    pub func now_utc() -> DateTime

    /// From Unix timestamp (seconds since epoch)
    pub func from_timestamp(ts: I64) -> DateTime

    /// From Unix timestamp with nanoseconds
    pub func from_timestamp_nanos(ts: I64, nanos: U32) -> DateTime

    /// To Unix timestamp
    pub func timestamp(this) -> I64

    /// To Unix timestamp with nanoseconds
    pub func timestamp_nanos(this) -> I128

    // Component accessors
    pub func year(this) -> I32
    pub func month(this) -> U8
    pub func day(this) -> U8
    pub func hour(this) -> U8
    pub func minute(this) -> U8
    pub func second(this) -> U8
    pub func nanosecond(this) -> U32

    /// Day of week (0=Sunday, 6=Saturday)
    pub func weekday(this) -> U8

    /// Day of year (1-366)
    pub func ordinal(this) -> U16

    /// Format using strftime-like format string
    pub func format(this, fmt: Str) -> Text

    /// Parse from string with format
    pub func parse(s: Str, fmt: Str) -> Outcome[DateTime, ParseError]

    /// Convert to UTC
    pub func to_utc(this) -> DateTime

    /// Convert to local time
    pub func to_local(this) -> DateTime
}
```

## Format Specifiers

| Specifier | Description | Example |
|-----------|-------------|---------|
| `%Y` | 4-digit year | `2024` |
| `%m` | Month (01-12) | `03` |
| `%d` | Day (01-31) | `15` |
| `%H` | Hour 24h (00-23) | `14` |
| `%M` | Minute (00-59) | `30` |
| `%S` | Second (00-59) | `45` |
| `%f` | Microseconds | `123456` |
| `%z` | UTC offset | `+0200` |
| `%Z` | Timezone name | `UTC` |
| `%a` | Short weekday | `Mon` |
| `%A` | Full weekday | `Monday` |
| `%b` | Short month | `Mar` |
| `%B` | Full month | `March` |

## Platform Implementation

### Windows

```c
// Instant::now()
QueryPerformanceCounter(&counter)

// SystemTime::now()
GetSystemTimePreciseAsFileTime(&ft)

// DateTime::now()
GetLocalTime(&st)
```

### Unix

```c
// Instant::now()
clock_gettime(CLOCK_MONOTONIC, &ts)

// SystemTime::now()
clock_gettime(CLOCK_REALTIME, &ts)

// DateTime::now()
time_t t = time(NULL)
localtime_r(&t, &tm)
```

## Example

```tml
use std::time::{Instant, Duration, DateTime}

func main() {
    // Measure execution time
    let start = Instant::now()
    expensive_work()
    let elapsed = start.elapsed()
    println("Took {elapsed.as_millis_f64():.2} ms")

    // Current time
    let now = DateTime::now()
    println("Current time: {now.format(\"%Y-%m-%d %H:%M:%S\")}")

    // Parse time
    let dt = DateTime::parse("2024-03-15 14:30:00", "%Y-%m-%d %H:%M:%S")!
    println("Parsed: {dt.format(\"%A, %B %d, %Y\")}")

    // Duration arithmetic
    let d1 = Duration::from_secs(60)
    let d2 = Duration::from_millis(500)
    let total = d1 + d2
    println("Total: {total.as_secs_f64()} seconds")
}
```

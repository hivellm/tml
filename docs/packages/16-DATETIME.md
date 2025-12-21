# TML Standard Library: Date and Time

> `std.datetime` — Date, time, and timezone handling.

## Overview

The datetime package provides types for representing and manipulating dates, times, durations, and timezones. It supports both local and UTC time, with full timezone database support.

**Capability**: `io.time` (for system clock access)

## Import

```tml
import std.datetime
import std.datetime.{DateTime, Date, Time, Duration, Timezone}
```

---

## Duration

Time span represented as seconds and nanoseconds.

```tml
/// A duration of time
public type Duration {
    secs: I64,
    nanos: U32,  // 0..999_999_999
}

extend Duration {
    /// Zero duration
    public const ZERO: Duration = Duration { secs: 0, nanos: 0 }

    /// Maximum duration
    public const MAX: Duration = Duration { secs: I64.MAX, nanos: 999_999_999 }

    // Constructors
    public func from_secs(secs: I64) -> Duration
    public func from_millis(millis: I64) -> Duration
    public func from_micros(micros: I64) -> Duration
    public func from_nanos(nanos: I64) -> Duration

    public func from_secs_f64(secs: F64) -> Duration
    public func from_secs_f32(secs: F32) -> Duration

    // Named constructors
    public func seconds(n: I64) -> Duration { Duration.from_secs(n) }
    public func milliseconds(n: I64) -> Duration { Duration.from_millis(n) }
    public func microseconds(n: I64) -> Duration { Duration.from_micros(n) }
    public func nanoseconds(n: I64) -> Duration { Duration.from_nanos(n) }
    public func minutes(n: I64) -> Duration { Duration.from_secs(n * 60) }
    public func hours(n: I64) -> Duration { Duration.from_secs(n * 3600) }
    public func days(n: I64) -> Duration { Duration.from_secs(n * 86400) }
    public func weeks(n: I64) -> Duration { Duration.from_secs(n * 604800) }

    // Accessors
    public func as_secs(this) -> I64
    public func as_millis(this) -> I128
    public func as_micros(this) -> I128
    public func as_nanos(this) -> I128
    public func as_secs_f64(this) -> F64
    public func as_secs_f32(this) -> F32

    public func subsec_millis(this) -> U32
    public func subsec_micros(this) -> U32
    public func subsec_nanos(this) -> U32

    // Predicates
    public func is_zero(this) -> Bool
    public func is_positive(this) -> Bool
    public func is_negative(this) -> Bool

    // Arithmetic
    public func abs(this) -> Duration
    public func saturating_add(this, other: Duration) -> Duration
    public func saturating_sub(this, other: Duration) -> Duration
    public func saturating_mul(this, n: I64) -> Duration
    public func checked_add(this, other: Duration) -> Option[Duration]
    public func checked_sub(this, other: Duration) -> Option[Duration]
    public func checked_mul(this, n: I64) -> Option[Duration]
    public func checked_div(this, n: I64) -> Option[Duration]
}

implement Add for Duration {
    type Output = Duration
    func add(this, other: Duration) -> Duration
}

implement Sub for Duration {
    type Output = Duration
    func sub(this, other: Duration) -> Duration
}

implement Mul[I64] for Duration {
    type Output = Duration
    func mul(this, n: I64) -> Duration
}

implement Div[I64] for Duration {
    type Output = Duration
    func div(this, n: I64) -> Duration
}

implement Neg for Duration {
    type Output = Duration
    func neg(this) -> Duration
}
```

---

## Instant

Monotonic clock for measuring elapsed time.

```tml
/// A point in time from a monotonic clock
public type Instant {
    secs: U64,
    nanos: U32,
}

extend Instant {
    /// Returns the current instant
    public func now() -> Instant
        caps: [io.time]

    /// Returns the elapsed time since this instant
    public func elapsed(this) -> Duration
        caps: [io.time]
    {
        Instant.now() - this
    }

    /// Returns the duration since an earlier instant
    public func duration_since(this, earlier: Instant) -> Duration {
        this - earlier
    }

    /// Returns the duration until a later instant
    public func duration_until(this, later: Instant) -> Duration {
        later - this
    }

    /// Adds a duration to this instant
    public func checked_add(this, duration: Duration) -> Option[Instant]

    /// Subtracts a duration from this instant
    public func checked_sub(this, duration: Duration) -> Option[Instant]
}

implement Add[Duration] for Instant {
    type Output = Instant
    func add(this, duration: Duration) -> Instant
}

implement Sub[Duration] for Instant {
    type Output = Instant
    func sub(this, duration: Duration) -> Instant
}

implement Sub for Instant {
    type Output = Duration
    func sub(this, other: Instant) -> Duration
}
```

---

## Date

Calendar date (year, month, day).

```tml
/// A calendar date
public type Date {
    year: I32,
    month: U8,   // 1..12
    day: U8,     // 1..31
}

extend Date {
    /// Creates a date from year, month, day
    public func new(year: I32, month: U8, day: U8) -> Option[Date] {
        if not Date.is_valid(year, month, day) then return None
        return Some(Date { year, month, day })
    }

    /// Creates a date, panicking on invalid input
    public func ymd(year: I32, month: U8, day: U8) -> Date {
        Date.new(year, month, day).expect("invalid date")
    }

    /// Returns today's date in UTC
    public func today_utc() -> Date
        caps: [io.time]

    /// Returns today's date in local timezone
    public func today_local() -> Date
        caps: [io.time]

    /// Creates a date from ordinal day of year
    public func from_ordinal(year: I32, day: U16) -> Option[Date]

    /// Creates a date from ISO week date
    public func from_iso_week(year: I32, week: U8, weekday: Weekday) -> Option[Date]

    /// Creates a date from Unix timestamp (days since 1970-01-01)
    public func from_unix_days(days: I64) -> Date

    // Accessors
    public func year(this) -> I32 { this.year }
    public func month(this) -> U8 { this.month }
    public func day(this) -> U8 { this.day }

    /// Returns the day of year (1..366)
    public func ordinal(this) -> U16

    /// Returns the weekday
    public func weekday(this) -> Weekday

    /// Returns the ISO week number (1..53)
    public func iso_week(this) -> U8

    /// Returns the ISO year (may differ from calendar year)
    public func iso_year(this) -> I32

    /// Returns the quarter (1..4)
    public func quarter(this) -> U8

    /// Returns true if this is a leap year
    public func is_leap_year(this) -> Bool {
        Date.is_leap_year_for(this.year)
    }

    /// Returns the number of days in this month
    public func days_in_month(this) -> U8

    /// Returns the number of days in this year
    public func days_in_year(this) -> U16 {
        if this.is_leap_year() then 366 else 365
    }

    // Arithmetic
    public func add_days(this, days: I64) -> Date
    public func add_months(this, months: I32) -> Date
    public func add_years(this, years: I32) -> Date

    public func sub_days(this, days: I64) -> Date
    public func sub_months(this, months: I32) -> Date
    public func sub_years(this, years: I32) -> Date

    /// Returns the number of days between two dates
    public func days_until(this, other: Date) -> I64

    // Predicates
    public func is_weekend(this) -> Bool
    public func is_weekday(this) -> Bool

    // Validation
    public func is_valid(year: I32, month: U8, day: U8) -> Bool
    public func is_leap_year_for(year: I32) -> Bool {
        (year % 4 == 0 and year % 100 != 0) or (year % 400 == 0)
    }

    // Formatting
    public func format(this, fmt: &String) -> String
    public func to_iso_string(this) -> String {
        this.format("%Y-%m-%d")
    }

    // Parsing
    public func parse(s: &String, fmt: &String) -> Result[Date, ParseError]
    public func parse_iso(s: &String) -> Result[Date, ParseError] {
        Date.parse(s, "%Y-%m-%d")
    }
}

implement Add[Duration] for Date {
    type Output = Date
    func add(this, duration: Duration) -> Date {
        this.add_days(duration.as_secs() / 86400)
    }
}

implement Sub for Date {
    type Output = Duration
    func sub(this, other: Date) -> Duration {
        Duration.days(this.days_until(other))
    }
}
```

### Weekday

```tml
/// Day of the week
public type Weekday = Monday | Tuesday | Wednesday | Thursday | Friday | Saturday | Sunday

extend Weekday {
    /// Returns the weekday from ISO number (1 = Monday, 7 = Sunday)
    public func from_iso(n: U8) -> Option[Weekday]

    /// Returns the weekday from Sunday-based number (0 = Sunday, 6 = Saturday)
    public func from_sunday_based(n: U8) -> Option[Weekday]

    /// Returns the ISO number (1..7)
    public func iso_number(this) -> U8

    /// Returns the Sunday-based number (0..6)
    public func sunday_based_number(this) -> U8

    /// Returns the next weekday
    public func succ(this) -> Weekday

    /// Returns the previous weekday
    public func pred(this) -> Weekday

    /// Returns the number of days until the given weekday
    public func days_until(this, target: Weekday) -> U8

    /// Returns the number of days since the given weekday
    public func days_since(this, target: Weekday) -> U8
}
```

### Month

```tml
/// Month of the year
public type Month = January | February | March | April | May | June |
                    July | August | September | October | November | December

extend Month {
    /// Returns the month from number (1..12)
    public func from_number(n: U8) -> Option[Month]

    /// Returns the month number (1..12)
    public func number(this) -> U8

    /// Returns the next month
    public func succ(this) -> Month

    /// Returns the previous month
    public func pred(this) -> Month

    /// Returns the number of days in this month for the given year
    public func days(this, year: I32) -> U8
}
```

---

## Time

Time of day.

```tml
/// A time of day
public type Time {
    hour: U8,     // 0..23
    minute: U8,   // 0..59
    second: U8,   // 0..59
    nano: U32,    // 0..999_999_999
}

extend Time {
    /// Creates a time from hours, minutes, seconds
    public func new(hour: U8, minute: U8, second: U8) -> Option[Time] {
        Time.new_nano(hour, minute, second, 0)
    }

    /// Creates a time with nanoseconds
    public func new_nano(hour: U8, minute: U8, second: U8, nano: U32) -> Option[Time]

    /// Creates a time, panicking on invalid input
    public func hms(hour: U8, minute: U8, second: U8) -> Time

    /// Returns midnight (00:00:00)
    public const MIDNIGHT: Time = Time { hour: 0, minute: 0, second: 0, nano: 0 }

    /// Returns noon (12:00:00)
    public const NOON: Time = Time { hour: 12, minute: 0, second: 0, nano: 0 }

    /// Returns the current time in UTC
    public func now_utc() -> Time
        caps: [io.time]

    /// Returns the current time in local timezone
    public func now_local() -> Time
        caps: [io.time]

    /// Creates a time from seconds since midnight
    public func from_secs(secs: U32) -> Option[Time]

    /// Creates a time from nanoseconds since midnight
    public func from_nanos(nanos: U64) -> Option[Time]

    // Accessors
    public func hour(this) -> U8 { this.hour }
    public func minute(this) -> U8 { this.minute }
    public func second(this) -> U8 { this.second }
    public func nanosecond(this) -> U32 { this.nano }
    public func millisecond(this) -> U16 { (this.nano / 1_000_000) as U16 }
    public func microsecond(this) -> U32 { this.nano / 1_000 }

    /// Returns seconds since midnight
    public func as_secs(this) -> U32 {
        this.hour as U32 * 3600 + this.minute as U32 * 60 + this.second as U32
    }

    /// Returns nanoseconds since midnight
    public func as_nanos(this) -> U64

    // Arithmetic
    public func add(this, duration: Duration) -> Time
    public func sub(this, duration: Duration) -> Time

    /// Returns the duration until another time (wraps at midnight)
    public func until(this, other: Time) -> Duration

    // Formatting
    public func format(this, fmt: &String) -> String
    public func to_iso_string(this) -> String {
        this.format("%H:%M:%S")
    }

    // Parsing
    public func parse(s: &String, fmt: &String) -> Result[Time, ParseError]
}
```

---

## DateTime

Combined date and time with optional timezone.

```tml
/// A date and time, optionally with timezone
public type DateTime {
    date: Date,
    time: Time,
    offset: Option[UtcOffset],
}

extend DateTime {
    /// Creates a DateTime from date and time
    public func new(date: Date, time: Time) -> DateTime {
        DateTime { date, time, offset: None }
    }

    /// Creates a DateTime with UTC offset
    public func with_offset(date: Date, time: Time, offset: UtcOffset) -> DateTime {
        DateTime { date, time, offset: Some(offset) }
    }

    /// Returns the current UTC datetime
    public func now_utc() -> DateTime
        caps: [io.time]

    /// Returns the current local datetime
    public func now_local() -> DateTime
        caps: [io.time]

    /// Creates from Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
    public func from_unix_timestamp(secs: I64) -> DateTime

    /// Creates from Unix timestamp with nanoseconds
    public func from_unix_timestamp_nanos(nanos: I128) -> DateTime

    /// Creates from Unix timestamp in milliseconds
    public func from_unix_timestamp_millis(millis: I64) -> DateTime

    // Accessors
    public func date(this) -> Date { this.date }
    public func time(this) -> Time { this.time }
    public func offset(this) -> Option[UtcOffset] { this.offset }

    public func year(this) -> I32 { this.date.year }
    public func month(this) -> U8 { this.date.month }
    public func day(this) -> U8 { this.date.day }
    public func hour(this) -> U8 { this.time.hour }
    public func minute(this) -> U8 { this.time.minute }
    public func second(this) -> U8 { this.time.second }
    public func nanosecond(this) -> U32 { this.time.nano }
    public func weekday(this) -> Weekday { this.date.weekday() }

    /// Returns the Unix timestamp
    public func unix_timestamp(this) -> I64

    /// Returns the Unix timestamp in milliseconds
    public func unix_timestamp_millis(this) -> I64

    /// Returns the Unix timestamp in nanoseconds
    public func unix_timestamp_nanos(this) -> I128

    // Timezone conversion
    /// Converts to UTC
    public func to_utc(this) -> DateTime

    /// Converts to a specific timezone
    public func to_timezone(this, tz: &Timezone) -> DateTime
        caps: [io.time]

    /// Converts to local timezone
    public func to_local(this) -> DateTime
        caps: [io.time]

    /// Replaces the offset without adjusting time
    public func with_offset_unchecked(this, offset: UtcOffset) -> DateTime

    // Arithmetic
    public func add(this, duration: Duration) -> DateTime
    public func sub(this, duration: Duration) -> DateTime
    public func duration_since(this, other: DateTime) -> Duration

    public func add_days(this, days: I64) -> DateTime
    public func add_months(this, months: I32) -> DateTime
    public func add_years(this, years: I32) -> DateTime

    // Formatting
    public func format(this, fmt: &String) -> String
    public func to_rfc2822(this) -> String
    public func to_rfc3339(this) -> String
    public func to_iso_string(this) -> String

    // Parsing
    public func parse(s: &String, fmt: &String) -> Result[DateTime, ParseError]
    public func parse_rfc2822(s: &String) -> Result[DateTime, ParseError]
    public func parse_rfc3339(s: &String) -> Result[DateTime, ParseError]
}

implement Add[Duration] for DateTime {
    type Output = DateTime
    func add(this, duration: Duration) -> DateTime
}

implement Sub[Duration] for DateTime {
    type Output = DateTime
    func sub(this, duration: Duration) -> DateTime
}

implement Sub for DateTime {
    type Output = Duration
    func sub(this, other: DateTime) -> Duration
}
```

---

## UtcOffset

Fixed UTC offset.

```tml
/// A fixed offset from UTC
public type UtcOffset {
    seconds: I32,  // -86400 < seconds < 86400
}

extend UtcOffset {
    /// UTC offset (0)
    public const UTC: UtcOffset = UtcOffset { seconds: 0 }

    /// Creates an offset from hours
    public func from_hours(hours: I8) -> Option[UtcOffset]

    /// Creates an offset from hours and minutes
    public func from_hms(hours: I8, minutes: I8, seconds: I8) -> Option[UtcOffset]

    /// Creates an offset from total seconds
    public func from_whole_seconds(seconds: I32) -> Option[UtcOffset]

    /// Returns the local system offset
    public func local() -> UtcOffset
        caps: [io.time]

    // Accessors
    public func whole_hours(this) -> I8
    public func whole_minutes(this) -> I8
    public func whole_seconds(this) -> I32 { this.seconds }

    public func is_utc(this) -> Bool { this.seconds == 0 }
    public func is_positive(this) -> Bool { this.seconds > 0 }
    public func is_negative(this) -> Bool { this.seconds < 0 }

    // Formatting
    public func format(this) -> String  // "+05:30" or "-08:00"
}

implement Neg for UtcOffset {
    type Output = UtcOffset
    func neg(this) -> UtcOffset {
        UtcOffset { seconds: -this.seconds }
    }
}
```

---

## Timezone

Full timezone with DST support.

```tml
/// A timezone with full DST rules
public type Timezone {
    // Internal: IANA timezone database
}

extend Timezone {
    /// UTC timezone
    public const UTC: Timezone = Timezone.from_str("UTC").unwrap()

    /// Local system timezone
    public func local() -> Timezone
        caps: [io.time]

    /// Creates a timezone from IANA name
    public func from_str(name: &String) -> Result[Timezone, TimezoneError]

    /// Creates a timezone from fixed offset
    public func from_offset(offset: UtcOffset) -> Timezone

    /// Returns the timezone name
    public func name(this) -> &String

    /// Returns the offset at a given UTC datetime
    public func offset_at(this, dt: DateTime) -> UtcOffset

    /// Returns the abbreviation at a given datetime (e.g., "PST", "PDT")
    public func abbreviation_at(this, dt: DateTime) -> String

    /// Returns true if DST is in effect at the given datetime
    public func is_dst_at(this, dt: DateTime) -> Bool

    /// Lists all available timezone names
    public func available() -> Vec[String]
}

/// Timezone error
public type TimezoneError = Unknown(String) | Ambiguous | NonExistent
```

---

## Format Specifiers

```
%Y    Year with century (2024)
%y    Year without century (24)
%m    Month (01..12)
%B    Full month name (January)
%b    Abbreviated month name (Jan)
%d    Day of month (01..31)
%e    Day of month, space-padded ( 1..31)
%j    Day of year (001..366)
%H    Hour, 24-hour (00..23)
%I    Hour, 12-hour (01..12)
%M    Minute (00..59)
%S    Second (00..59)
%f    Microseconds (000000..999999)
%3f   Milliseconds (000..999)
%9f   Nanoseconds
%p    AM/PM
%P    am/pm
%Z    Timezone abbreviation (PST)
%z    Timezone offset (+0800)
%:z   Timezone offset with colon (+08:00)
%A    Full weekday name (Sunday)
%a    Abbreviated weekday name (Sun)
%w    Weekday number (0 = Sunday)
%u    ISO weekday number (1 = Monday)
%U    Week number, Sunday start (00..53)
%W    Week number, Monday start (00..53)
%V    ISO week number (01..53)
%G    ISO year
%%    Literal %
%n    Newline
%t    Tab
```

---

## Examples

### Current Time

```tml
import std.datetime.{DateTime, Date, Time, Duration}

func show_current_time()
    caps: [io.time]
{
    let now = DateTime.now_local()
    print("Current time: " + now.format("%Y-%m-%d %H:%M:%S %Z"))
    print("Unix timestamp: " + now.unix_timestamp().to_string())

    let today = Date.today_local()
    print("Today is " + today.weekday().to_string())
    print("Day " + today.ordinal().to_string() + " of " + today.year().to_string())
}
```

### Duration Calculations

```tml
import std.datetime.{DateTime, Duration}

func time_operations() {
    let start = DateTime.now_utc()

    // Do some work...

    let elapsed = DateTime.now_utc() - start
    print("Elapsed: " + elapsed.as_millis().to_string() + "ms")

    // Schedule future event
    let future = start + Duration.hours(2) + Duration.minutes(30)
    print("Event at: " + future.to_iso_string())
}
```

### Date Arithmetic

```tml
import std.datetime.{Date, Weekday}

func date_calculations() {
    let today = Date.ymd(2024, 3, 15)

    // Add/subtract days
    let tomorrow = today.add_days(1)
    let last_week = today.sub_days(7)

    // Add months (handles month-end correctly)
    let next_month = today.add_months(1)

    // Find next Monday
    let days_to_monday = today.weekday().days_until(Weekday.Monday)
    let next_monday = today.add_days(days_to_monday as I64)

    // Days between dates
    let new_year = Date.ymd(2025, 1, 1)
    let days_until = today.days_until(new_year)
    print("Days until new year: " + days_until.to_string())
}
```

### Timezone Handling

```tml
import std.datetime.{DateTime, Timezone, UtcOffset}

func timezone_example()
    caps: [io.time]
{
    let utc_now = DateTime.now_utc()

    // Convert to specific timezone
    let tokyo = Timezone.from_str("Asia/Tokyo").unwrap()
    let tokyo_time = utc_now.to_timezone(&tokyo)
    print("Tokyo: " + tokyo_time.format("%Y-%m-%d %H:%M:%S %Z"))

    let new_york = Timezone.from_str("America/New_York").unwrap()
    let ny_time = utc_now.to_timezone(&new_york)
    print("New York: " + ny_time.format("%Y-%m-%d %H:%M:%S %Z"))

    // Check if DST is in effect
    if new_york.is_dst_at(ny_time) then {
        print("Daylight Saving Time is in effect")
    }
}
```

### Parsing and Formatting

```tml
import std.datetime.{DateTime, Date}

func parsing_example() {
    // Parse ISO 8601
    let dt = DateTime.parse_rfc3339("2024-03-15T10:30:00Z").unwrap()

    // Parse custom format
    let date = Date.parse("March 15, 2024", "%B %d, %Y").unwrap()

    // Format for display
    print(dt.format("%A, %B %d, %Y at %I:%M %p"))
    // "Friday, March 15, 2024 at 10:30 AM"

    // Format for API
    print(dt.to_rfc3339())
    // "2024-03-15T10:30:00Z"
}
```

---

## See Also

- [std.async](./14-ASYNC.md) — Async sleep and timeout
- [05-SEMANTICS.md](../specs/05-SEMANTICS.md) — Capability system

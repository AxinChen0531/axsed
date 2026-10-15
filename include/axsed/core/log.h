#ifndef AXSED_LOG_H
#define AXSED_LOG_H

#include <axsed/core/base.h>
#include <chrono>
#include <format>
#include <functional>
#include <mutex>
#include <shared_mutex>

#define AX_TRACE(...)   axsed::ax_get_default_logger()->log(axsed::LOGLEVEL_TRACE, __VA_ARGS__)
#define AX_DEBUG(...)   axsed::ax_get_default_logger()->log(axsed::LOGLEVEL_DEBUG, __VA_ARGS__)
#define AX_INFO(...)    axsed::ax_get_default_logger()->log(axsed::LOGLEVEL_INFO, __VA_ARGS__)
#define AX_WARN(...)    axsed::ax_get_default_logger()->log(axsed::LOGLEVEL_WARN, __VA_ARGS__)
#define AX_ERROR(...)   axsed::ax_get_default_logger()->log(axsed::LOGLEVEL_ERROR, __VA_ARGS__)
#define AX_FATAL(...)   axsed::ax_get_default_logger()->log(axsed::LOGLEVEL_FATAL, __VA_ARGS__)

namespace axsed
{
    enum LOGLEVEL_ : int
    {
        LOGLEVEL_TRACE = 0,
        LOGLEVEL_DEBUG = 1,
        LOGLEVEL_INFO  = 2,
        LOGLEVEL_WARN  = 3,
        LOGLEVEL_ERROR = 4,
        LOGLEVEL_FATAL = 5,
    };

    /**
     * log meta data, for log system using
     */
    struct AXSEDCORE_API LogMeta
    {
        std::string_view Message;
        std::string_view LoggerName;
        std::chrono::zoned_time<std::chrono::nanoseconds> ZonedTime;
        uint64_t ThreadID;
        int ProcessID;
        LOGLEVEL_ Level;
    };

    /**
     * derive to custom logger's sink
     */
    class AXSEDCORE_API ILogSink
    {
    public:
        virtual ~ILogSink() = default;
        /**
         * override to provide the entry point of loggers' log
         * @param msg the log content composited with pattern
         * @attention user should promise the multi-thread safety
         */
        virtual void sinkIt(const LogMeta& msg) = 0;
    };

    class Logger;
    /**
     * default logger's default sink
     * to output colored text to terminal
     */
    class AXSEDCORE_API StdoutColorSink final : public ILogSink
    {
    public:
        explicit StdoutColorSink();
        ~StdoutColorSink() override;
        void sinkIt(const LogMeta& msg) override;

        static Logger* GenerateLogger(std::string_view loggerName, LOGLEVEL_ level = LOGLEVEL_TRACE);
    };

    /**
     * multi-safe by ReadWriteLock, setting function as Write, log function as Read
     */
    class AXSEDCORE_API Logger final
    {
        using LogEmitter = std::function<void(const LogMeta& meta, String& composite)>;

    public:
        /**
         * @param loggerName the logger's name
         * @param level the logger's minium processing level
         */
        explicit Logger(std::string_view loggerName, LOGLEVEL_ level = LOGLEVEL_TRACE);

        /**
         * @attention multi-thread unsafe, make sure no log function calling before destroy
         */
        ~Logger();

        /**
         * get the logger's minium processing level
         * @attention multi-thread safe
         */
        [[nodiscard]] LOGLEVEL_ getLevel() const noexcept;
        /**
         * get log msg composite pattern
         * default is "[$ymd$ $cms$] $msg$"
         * @attention multi-thread safe
         */
        [[nodiscard]] String getPattern() const noexcept;
        /**
         * set the logger's minium processing level
         * default is "[$ymd$ $cms$] $msg$"
         * @param level the minium level
         * @attention multi-thread safe
         */
        void setLevel(LOGLEVEL_ level) noexcept;
        /**
         * set log msg composite pattern
         * @param pattern the composite rule, ref to note
         * @attention multi_thread safe
         * @note use $token$ to change custom composite rule, token list:
         * - $INVALID$ - SPECIAL, INVALID mean no registry token, log system will write the $INVALID$ string directly
         * - $NO_BACKTRACE_FLAG - SPACIAL, when control flag $ without backtrace, logger will write the whole $NO_BACKTRACE_FLAG string directly
         * - $$ - SPECIAL, nothing between the control flag to write the '$' character
         * - $msg$ - the raw message content
         * - $ln$ - the logger's name
         * - $tid$ - the thread id
         * - $pid$ - the process id
         * - $lvf$ - the full log level name: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
         * - $lvs$ - the simplified level name: T, D, I, W, E, F
         * - $yr$ - the 4 digital format year, in current timezone
         * - $yr2$ - the 2 digital format year, in current timezone
         * - $mt$ - the 2 digital format month, 01 for January, in current timezone
         * - $mtf$ - the month's fullname, in current timezone
         * - $mts$ - the month's simplified name, in current timezone
         * - $wd$ - the weekday in number, 0 for sunday, 6 for Saturday, in current timezone
         * - $wd1$ - the weekday in number, 1 for monday, 7 for Sunday, in current timezone
         * - $wdf$ - the weekday's fullname, in current timezone
         * - $wds$ - the weekday's simplified name, in current timezone
         * - $dom$ - the 2 digital day of month, 01 for 1st, in current timezone
         * - $doy$ - the day of year in 3 digital, 001 for 1st January, in current timezone
         * - $wy$ - the week of year in 2 digital, first Sunday of the year as the 01 week's first day, before is 00 week, in current timezone
         * - $wy1$ - the week of year in 2 digital, first Monday of the year as the 01 week's first day, before is 00 week, in current timezone
         * - $ymd$ - the regular date format, equal to $yr$-$mt$-$dom$ but with higher performance
         * - $hr$ - the hour of the day [0, 24) in 2 digital, in current timezone
         * - $hr2$ - the hour of the day [0, 12) in 2 digital, in current timezone
         * - $hrf$ - AM/PM flag, in current timezone
         * - $min$ - the minutes of the hour [0, 60) in 2 digital, in current timezone
         * - $sec$ - the seconds of the minute [0, 60) in 2 digital, in current timezone
         * - $msc$ - the milliseconds of the second [0, 1000) in 2 digital, in current timezone
         * - $csc$ - the clock to seconds, equal to $hr$:$min$:$sec$, but with higher performance
         * - $cms$ - the clock to milliseconds, equal to $hr$:$min$:$sec$.$msc$, but with higher performance
         * - $ofs$ - the offset between current timezone and utc in 4 digital with ± sign, meet ISO-8601 format, ep: +0830 (mean 8 hours 30 minutes earlier than utc)
         * - $tmz$ - the current timezone's name
         */
        void setPattern(std::string_view pattern);
        /**
         * push a sink to logger
         * @param sink the msg target terminal
         * @attention multi-thread safe
         */
        void pushSink(Scope<ILogSink>&& sink);
        /**
         * pop a sink out of logger
         * @attention multi-thread safe
         */
        void popSink();

        /**
         * log a msg
         * @param level the log level
         * @param rawMsg the log content without pattern
         * @attention multi-thread safe
         */
        void log(LOGLEVEL_ level, String&& rawMsg) const;

        /**
         * log a formatting msg
         * @param level log level
         * @param fmt the formatting string
         * @param args the formatting arguments
         * @attention multi-thread safe
         */
        template<typename... Args>
        void log(const LOGLEVEL_ level, const std::format_string<Args...> fmt, Args&&... args) const {
            String s;
            std::format_to(std::back_inserter(s), fmt, std::forward<Args>(args)...);
            this->log(level, std::move(s));
        }

    private:
        mutable std::shared_mutex m_settingLock;
        String m_pattern;
        Vector<LogEmitter> m_patternEntry;
        Vector<Scope<ILogSink>> m_sinks;
        String m_name;
        std::atomic<LOGLEVEL_> m_level;
    };

    /**
     * get the default logger
     * macro AX_TRACE AX_DEBUG AX_INFO AX_WARN AX_ERROR AX_FATAL will using the default logger
     * have a default Logger with StdoutColorSink, set to nullptr to save some memory(NOT RECOMMENDED)
     */
    AXSEDCORE_API Scope<Logger>& ax_get_default_logger();
    /**
     * default logger setting, will delete the old one.
     * macro AX_TRACE AX_DEBUG AX_INFO AX_WARN AX_ERROR AX_FATAL will using the default logger
     * have a default Logger with StdoutColorSink, set to nullptr to save some memory(NOT RECOMMENDED)
     * @attention multi-thread unsafe
     * @param logger
     */
    AXSEDCORE_API void ax_set_default_logger(Scope<Logger>&& logger);
}

#endif //AXSED_LOG_H

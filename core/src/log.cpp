#include <axsed/core/log.h>

namespace axsed
{
    StdoutColorSink::StdoutColorSink() = default;
    StdoutColorSink::~StdoutColorSink() = default;

    void StdoutColorSink::sinkIt(const LogMeta& msg) {
        switch (msg.Level) {
            case LOGLEVEL_TRACE: fprintf(stdout, "\033[2;37m%s\033[0m\n", msg.Message.data());
                break;
            case LOGLEVEL_DEBUG: fprintf(stdout, "\033[1;37m%s\033[0m\n", msg.Message.data());
                break;
            case LOGLEVEL_INFO: fprintf(stdout, "\033[1;32m%s\033[0m\n", msg.Message.data());
                break;
            case LOGLEVEL_WARN: fprintf(stdout, "\033[1;33m%s\033[0m\n", msg.Message.data());
                break;
            case LOGLEVEL_ERROR: fprintf(stdout, "\033[1;31m%s\033[0m\n", msg.Message.data());
                break;
            case LOGLEVEL_FATAL: fprintf(stdout, "\033[1;37;41m%s\033[0m\n", msg.Message.data());
                break;
        }
    }

    Logger* StdoutColorSink::GenerateLogger(std::string_view loggerName, LOGLEVEL_ level) {
        auto* ans = ax_new<Logger>(loggerName, level);
        ans->pushSink(ax_make_scope<StdoutColorSink>());
        return ans;
    }

    static UnorderedMap<String, std::function<void(const LogMeta&, String&)>> ax_get_registry_log_emitters() {
        using namespace std::chrono;
        return UnorderedMap<String, std::function<void(const LogMeta&, String&)>>{
            {
                "",
                [](const LogMeta&, String& composite) -> void {
                    composite.push_back('$');
                }
            },
            {
                "msg",
                [](const LogMeta& meta, String& composite) -> void {
                    composite.append(meta.Message);
                }
            },
            {
                "ln",
                [](const LogMeta& meta, String& composite) -> void {
                    composite.append(meta.LoggerName);
                }
            },
            {
                "tid",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{}", meta.ThreadID);
                }
            },
            {
                "pid",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{}", meta.ProcessID);
                }
            },
            {
                "lvf",
                [](const LogMeta& meta, String& composite) -> void {
                    switch (meta.Level) {
                        case LOGLEVEL_TRACE: composite.append("TRACE");
                            break;
                        case LOGLEVEL_DEBUG: composite.append("DEBUG");
                            break;
                        case LOGLEVEL_INFO: composite.append("INFO");
                            break;
                        case LOGLEVEL_WARN: composite.append("WARN");
                            break;
                        case LOGLEVEL_ERROR: composite.append("ERROR");
                            break;
                        case LOGLEVEL_FATAL: composite.append("FATAL");
                            break;
                    }
                }
            },
            {
                "lvs",
                [](const LogMeta& meta, String& composite) -> void {
                    switch (meta.Level) {
                        case LOGLEVEL_TRACE: composite.push_back('T');
                            break;
                        case LOGLEVEL_DEBUG: composite.push_back('D');
                            break;
                        case LOGLEVEL_INFO: composite.push_back('I');
                            break;
                        case LOGLEVEL_WARN: composite.push_back('W');
                            break;
                        case LOGLEVEL_ERROR: composite.push_back('E');
                            break;
                        case LOGLEVEL_FATAL: composite.push_back('F');
                            break;
                    }
                }
            },
            {
                "yr",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%Y}", meta.ZonedTime);
                }
            },
            {
                "yr2",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%y}", meta.ZonedTime);
                }
            },
            {
                "mt",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%m}", meta.ZonedTime);
                }
            },
            {
                "mtf",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%B}", meta.ZonedTime);
                }
            },
            {
                "mts",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%b}", meta.ZonedTime);
                }
            },
            {
                "wd",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%w}", meta.ZonedTime);
                }
            },
            {
                "wd1",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%u}", meta.ZonedTime);
                }
            },
            {
                "wdf",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%A}", meta.ZonedTime);
                }
            },
            {
                "wds",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%a}", meta.ZonedTime);
                }
            },
            {
                "dom",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%d}", meta.ZonedTime);
                }
            },
            {
                "doy",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%j}", meta.ZonedTime);
                }
            },
            {
                "wy",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%U}", meta.ZonedTime);
                }
            },
            {
                "wy1",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%W}", meta.ZonedTime);
                }
            },
            {
                "ymd",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%F}", meta.ZonedTime);
                }
            },
            {
                "h24",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%H}", meta.ZonedTime);
                }
            },
            {
                "h12",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%I}", meta.ZonedTime);
                }
            },
            {
                "hf",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%p}", meta.ZonedTime);
                }
            },
            {
                "min",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%M}", meta.ZonedTime);
                }
            },
            {
                "sec",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(
                        std::back_inserter(composite),
                        "{:02d}",
                        time_point_cast<seconds>(meta.ZonedTime.get_local_time()).time_since_epoch().count() % 60
                    );
                }
            },
            {
                "msc",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(
                        std::back_inserter(composite),
                        "{:03d}",
                        time_point_cast<milliseconds>(meta.ZonedTime.get_local_time()).time_since_epoch().count() % 1000
                    );
                }
            },
            {
                "csc",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(
                        std::back_inserter(composite),
                        "{:%X}",
                        time_point_cast<milliseconds>(meta.ZonedTime.get_local_time())
                    );
                }
            },
            {
                "cms",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(
                        std::back_inserter(composite),
                        "{:%T}",
                        time_point_cast<milliseconds>(meta.ZonedTime.get_local_time())
                    );
                }
            },
            {
                "ofs",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{:%z}", meta.ZonedTime);
                }
            },
            {
                "tmz",
                [](const LogMeta& meta, String& composite) -> void {
                    std::format_to(std::back_inserter(composite), "{}", meta.ZonedTime.get_time_zone()->name());
                }
            },
        };
    }

    static Scope<Logger> ax_default_logger(StdoutColorSink::GenerateLogger("axsed"));

    Logger::Logger(const std::string_view loggerName, const LOGLEVEL_ level)
        : m_name(loggerName), m_level(level) {
        setPattern("[$ymd$ $cms$] $msg$");
    }

    Logger::~Logger() = default;

    LOGLEVEL_ Logger::getLevel() const noexcept {
        return m_level.load(std::memory_order_acquire);
    }

    String Logger::getPattern() const noexcept {
        std::unique_lock lock(m_settingLock);
        return m_pattern;
    }

    void Logger::setLevel(const LOGLEVEL_ level) noexcept {
        m_level.store(level, std::memory_order_relaxed);
    }

    void Logger::setPattern(const std::string_view pattern) {
        UnorderedMap<String, LogEmitter> registry = ax_get_registry_log_emitters();
        std::unique_lock lock(m_settingLock);
        m_pattern.assign(pattern);
        m_patternEntry.clear();
        size_t i = 0;
        while (i < m_pattern.size()) {
            if (m_pattern[i] == '$') {
                const size_t end = m_pattern.find('$', i + 1);
                if (end == std::string_view::npos) {
                    // ref $NO_BACKTRACE_FLAG
                    m_patternEntry.emplace_back(
                        [this, i](const LogMeta&, String& composite) -> void {
                            composite.append(std::string_view(m_pattern.data() + i, m_pattern.size() - i));
                        }
                    );
                    break;
                }
                const String token(m_pattern.data() + i + 1, end - i - 1);
                if (const auto it = registry.find(token); it != registry.end()) {
                    // ref $xxx$
                    m_patternEntry.emplace_back(it->second);
                } else {
                    // ref $INVALID$
                    m_patternEntry.emplace_back(
                        [this, end, i](const LogMeta&, String& composite) -> void {
                            composite.append(std::string_view(m_pattern.data() + i, end - i + 1));
                        }
                    );
                }
                i = end + 1;
            } else {
                // ref no token part
                if (const size_t next = m_pattern.find('$', i + 1); next == std::string_view::npos) {
                    m_patternEntry.emplace_back(
                        [this, i](const LogMeta&, String& composite) -> void {
                            composite.append(std::string_view(m_pattern.data() + i, m_pattern.size() - i));
                        }
                    );
                    break;
                } else {
                    m_patternEntry.emplace_back(
                        [this, next, i](const LogMeta&, String& composite) -> void {
                            composite.append(std::string_view(m_pattern.data() + i, next - i));
                        }
                    );
                    i = next;
                }
            }
        }
    }

    void Logger::pushSink(Scope<ILogSink>&& sink) {
        std::unique_lock lock(m_settingLock);
        m_sinks.emplace_back(std::move(sink));
    }

    void Logger::popSink() {
        std::unique_lock lock(m_settingLock);
        if (!m_sinks.empty())
            m_sinks.pop_back();
    }

    void Logger::log(const LOGLEVEL_ level, String&& rawMsg) const {
        using namespace std::chrono;
        if (level < m_level)
            return;
        LogMeta msgStruct{
            .Message = rawMsg,
            .LoggerName = m_name,
            .ZonedTime = zoned_time(current_zone(), system_clock::now()),
            .ThreadID = ax_get_tid(),
            .ProcessID = ax_get_pid(),
            .Level = level,
        };
        String msgWithPattern;
        std::shared_lock lock(m_settingLock);
        for (const LogEmitter& pattern : m_patternEntry) {
            pattern(msgStruct, msgWithPattern);
        }
        msgStruct.Message = msgWithPattern;
        for (const Scope<ILogSink>& sink : m_sinks)
            sink->sinkIt(msgStruct);
    }

    Scope<Logger>& ax_get_default_logger() {
        return ax_default_logger;
    }

    void ax_set_default_logger(Scope<Logger>&& logger) {
        ax_default_logger = std::move(logger);
    }
}

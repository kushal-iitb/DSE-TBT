#include "logging_object.hpp"



namespace{
    std::once_flag logger_init_once;
    std::atomic<DSE::logger::LoggerT*> loggerptr{nullptr};
    int logger_backend_err_fd = -1;

    void open_backend_error_file(std::filesystem::path const& log_dir){
        std::error_code ec;
        std::filesystem::create_directories(log_dir , ec);

        auto const p = (log_dir / "quill_backend_errors.log").string();
        logger_backend_err_fd = ::open(p.c_str() , O_CREAT | O_WRONLY | O_APPEND , 0644);
    }

    void backend_error_notifier( std::string const& msg){
        if(logger_backend_err_fd < 0)
        return;

        ssize_t n;
        n = ::write(logger_backend_err_fd , msg.data() , msg.size()); (void)n;
        n = ::write(logger_backend_err_fd , "\n" , 1);                (void)n;
    }

    std::string current_datetime_local(){
        auto const now = std::chrono::system_clock::now();
        std::time_t t  = std::chrono::system_clock::to_time_t(now);
        std::tm tm_local{};
        ::localtime_r(&t , &tm_local);

        char buf[16]= {0};
        if(::strftime(buf , sizeof(buf) , "%d%m%Y_%H%M%S" , & tm_local) == 0)
        return "";

        return std::string(buf);


    }


} // namespace

void Logger::init(std::string program_name , std::string log_dir , std::uint16_t backend_cpu_affinity){

        std::call_once(logger_init_once , [&](){
            std::filesystem::path const dir{log_dir};
            open_backend_error_file(dir);

            quill::BackendOptions backend_options;
            backend_options.thread_name = "dseLoggerBackend";
            backend_options.sleep_duration = std::chrono::nanoseconds{0};
            backend_options.enable_yield_when_idle = false;
            backend_options.error_notifier = backend_error_notifier;
            backend_options.cpu_affinity = backend_cpu_affinity;

            quill::Backend::start<DSE::logger::dseFrontendOptions>(backend_options , quill::SignalHandlerOptions{});

            std::string const datetime = current_datetime_local();
            std::string const filename = datetime.empty() ? (program_name + ".logs") : (program_name + "_" + datetime +  ".log");


            auto file_sink = quill::FrontendImpl<DSE::logger::dseFrontendOptions>::create_or_get_sink<quill::FileSink>(
                (dir/filename).string(),
                [](){
                    quill::FileSinkConfig cfg;
                    cfg.set_open_mode('w');
                    cfg.set_filename_append_option(quill::FilenameAppendOption::None);
                    return cfg;
                }(),
                quill::FileEventNotifier{}
            );


            quill::PatternFormatterOptions pfo;
            pfo.format_pattern = 
                "%(time) [%(thread_id)] %(source_location:<28) LOG_%(log_level:<9) %(message)";
            pfo.timestamp_pattern = "%H:%M:%S.%Qns";
            pfo.timestamp_timezone = quill::Timezone::LocalTime;

            auto* lg = quill::FrontendImpl<DSE::logger::dseFrontendOptions>::create_or_get_logger(program_name , std::shared_ptr<quill::Sink>{file_sink} , pfo);

            lg->set_log_level(quill::LogLevel::Info);

            loggerptr.store(lg , std::memory_order_release);

        });
}

DSE::logger::LoggerT* Logger::get() noexcept{
    return loggerptr.load(std::memory_order_acquire);
}

void Logger::flush() noexcept{

    if(auto* lg = get(); lg){
        lg->flush_log();
    }
}
#pragma once

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

#include<atomic>
#include<filesystem>


#include<chrono>


namespace DSE :: logger {

// Low latency tuned frontend options
struct dseFrontendOptions{
    static constexpr quill::QueueType queue_type = quill::QueueType::BoundedDropping;
    static constexpr size_t initial_queue_capacity = 4u * 1024u * 1024u;
    static constexpr size_t unbounded_queue_max_capacity = 0u;
    static constexpr quill::HugePagesPolicy huge_pages_policy = quill::HugePagesPolicy::Try;
};

using LoggerT = quill::LoggerImpl<dseFrontendOptions>;

} //namespace DSE::logger



    class Logger final{

        public:
        static void init(std::string program_name , std::string log_dir = "logs" , std::uint16_t backend_cpu_affinity=3);
        static DSE::logger::LoggerT* get() noexcept;
        static void flush() noexcept;
        
    };

    #define DSE_LOG_INFO(fmt, ...)                          \
    do{                                                     \
        auto* logger = Logger::get();                        \
        if(logger){                                         \
            LOG_INFO(logger, fmt __VA_OPT__(,) __VA_ARGS__); \
        }                                                   \
    } while(0)                                      


    #define DSE_LOG_DEBUG(fmt, ...)                          \
    do{                                                     \
        auto* logger = Logger::get();                        \
        if(logger){                                         \
            LOG_DEBUG(logger, fmt __VA_OPT__(,) __VA_ARGS__); \
        }                                                   \
    } while(0)
    
    #define DSE_LOG_ERROR(fmt, ...)                          \
    do{                                                     \
        auto* logger = Logger::get();                        \
        if(logger){                                         \
            LOG_ERROR(logger, fmt __VA_OPT__(,) __VA_ARGS__); \
        }                                                   \
    } while(0)                                                                              

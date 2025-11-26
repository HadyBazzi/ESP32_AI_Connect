// ESP32_AI_Connect/ESP32_AI_Connect_config.h
// =============================================================================
// CONFIGURATION OVERRIDE INSTRUCTIONS
// =============================================================================
// All options below can be overridden WITHOUT editing this file:
//
// PlatformIO (platformio.ini):
//   build_flags =
//     -DDISABLE_<OPTION>
//     -D<OPTION>=<NEW VALUE>
//
// Arduino IDE (in your .ino file, BEFORE #include <ESP32_AI_Connect.h>):
//   #define DISABLE_<OPTION>
//   #define <OPTION> <NEW VALUE>
//   #include <ESP32_AI_Connect.h>
// =============================================================================

#ifndef ESP32_AI_CONNECT_CONFIG_H
#define ESP32_AI_CONNECT_CONFIG_H

// --- Debug Options ---
// Debug output is ENABLED by default.
// To disable: define DISABLE_DEBUG_OUTPUT before including the library
// or use build flag: -DDISABLE_DEBUG_OUTPUT
#ifndef DISABLE_DEBUG_OUTPUT
#define ENABLE_DEBUG_OUTPUT
#endif

// --- Tool Calls Support ---
// Tool calls (function calling) support is ENABLED by default.
// To disable: define DISABLE_TOOL_CALLS before including the library
// or use build flag: -DDISABLE_TOOL_CALLS
// Disabling saves memory if you don't need tcChat/tcReply methods.
#ifndef DISABLE_TOOL_CALLS
#define ENABLE_TOOL_CALLS
#endif

// --- Streaming Chat Support ---
// Streaming chat is ENABLED by default.
// To disable: define DISABLE_STREAM_CHAT before including the library
// or use build flag: -DDISABLE_STREAM_CHAT
// Disabling saves memory if you don't need streamChat methods.
#ifndef DISABLE_STREAM_CHAT
#define ENABLE_STREAM_CHAT
#endif

// --- Streaming Configuration ---
// Configure streaming chat behavior (only used when ENABLE_STREAM_CHAT is defined)
// These values can be overridden by defining them before including the library
// or via build flags: -DSTREAM_CHAT_CHUNK_SIZE=1024

#ifndef STREAM_CHAT_CHUNK_SIZE
#define STREAM_CHAT_CHUNK_SIZE 512        // Size of each HTTP read chunk
#endif

#ifndef STREAM_CHAT_CHUNK_TIMEOUT_MS
#define STREAM_CHAT_CHUNK_TIMEOUT_MS 5000 // Timeout for each chunk read
#endif

// --- Platform Selection ---
// All platforms are ENABLED by default.
// To disable a platform: define DISABLE_AI_API_<PLATFORM> before including
// or use build flags: -DDISABLE_AI_API_<PLATFORM>
// Disabling unused platforms saves code space.

#ifndef DISABLE_AI_API_OPENAI
#define USE_AI_API_OPENAI        // Enable OpenAI and OpenAI-compatible APIs
#endif

#ifndef DISABLE_AI_API_GEMINI
#define USE_AI_API_GEMINI        // Enable Google Gemini API
#endif

#ifndef DISABLE_AI_API_DEEPSEEK
#define USE_AI_API_DEEPSEEK      // Enable DeepSeek API
#endif

#ifndef DISABLE_AI_API_CLAUDE
#define USE_AI_API_CLAUDE        // Enable Anthropic Claude API
#endif
// Add other platforms here as needed

// --- Advanced Configuration ---
// These values can be overridden by defining them before including the library
// or via build flags: -DAI_API_REQ_JSON_DOC_SIZE=1024

#ifndef AI_API_REQ_JSON_DOC_SIZE
#define AI_API_REQ_JSON_DOC_SIZE 5120
#endif

#ifndef AI_API_RESP_JSON_DOC_SIZE
#define AI_API_RESP_JSON_DOC_SIZE 2048
#endif

#ifndef AI_API_HTTP_TIMEOUT_MS
#define AI_API_HTTP_TIMEOUT_MS 30000 // 30 seconds
#endif

#endif // ESP32_AI_CONNECT_CONFIG_H
#include "AI_API_Claude.h"

// Constructor
AI_API_Claude_Handler::AI_API_Claude_Handler() {
    // Any initialization needed
}

// Destructor
AI_API_Claude_Handler::~AI_API_Claude_Handler() {
    // Any cleanup needed
}

// Get API endpoint for Claude
String AI_API_Claude_Handler::getEndpoint(const String& modelName, const String& apiKey, const String& customEndpoint) const {
    if (customEndpoint.length() > 0) {
        return customEndpoint;
    }
    return "https://api.anthropic.com/v1/messages";
}

// Set headers for Claude API
void AI_API_Claude_Handler::setHeaders(HTTPClient& httpClient, const String& apiKey) {
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("x-api-key", apiKey);
    httpClient.addHeader("anthropic-version", _apiVersion);
}

// Build request body for Claude API
String AI_API_Claude_Handler::buildRequestBody(const String& modelName, const String& systemRole,
                                             float temperature, int maxTokens,
                                             const String& userMessage, JsonDocument& doc,
                                             const String& customParams) {
    try {
        // Set the model
        doc["model"] = modelName;
        
        // Process custom parameters if provided
        if (customParams.length() > 0) {
            // Create a temporary document to parse the custom parameters
            JsonDocument paramsDoc;
            DeserializationError error = deserializeJson(paramsDoc, customParams);
            
            // Only proceed if parsing was successful
            if (!error) {
                // Add each parameter from customParams to the request
                for (JsonPair param : paramsDoc.as<JsonObject>()) {
                    // Skip model, messages, system as they are handled separately
                    if (param.key() != "model" && param.key() != "messages" && param.key() != "system") {
                        // Copy the parameter to our request document
                        doc[param.key()] = param.value();
                    }
                }
            }
        }
        
        // Add optional parameters (these override any custom parameters)
        if (temperature >= 0) {
            doc["temperature"] = temperature;
        }
        
        // IMPORTANT: Claude API requires 'max_tokens' field - it cannot be omitted
        // According to Anthropic documentation: https://docs.anthropic.com/en/api/messages
        // The 'max_tokens' field is required and cannot be left empty
        // Use provided value if > 0, otherwise use default of 1024
        doc["max_tokens"] = (maxTokens > 0) ? maxTokens : 1024;
        
        // Add system message if specified (only if user has set it with setTCChatSystemRole)
        if (systemRole.length() > 0) {
            doc["system"] = systemRole;
        }
        
        // Create messages array with user message
        JsonArray messages = doc["messages"].to<JsonArray>();
        JsonObject userMsg = messages.add<JsonObject>();
        userMsg["role"] = "user";
        userMsg["content"] = userMessage;
        
        // Serialize the request body
        String requestBody;
        serializeJson(doc, requestBody);
        return requestBody;
    } 
    catch (const std::exception& e) {
        return "";
    }
}

// Parse response body from Claude API
String AI_API_Claude_Handler::parseResponseBody(const String& responsePayload,
                                              String& errorMsg, JsonDocument& doc) {
    resetState();  // Reset finish reason and token count
    
    try {
        // Parse the JSON response
        DeserializationError error = deserializeJson(doc, responsePayload);
        if (error) {
            errorMsg = "JSON parsing error: " + String(error.c_str());
            return "";
        }
        
        // Check for error in the response
        if (!doc["error"].isNull()) {
            if (!doc["error"]["message"].isNull()) {
                errorMsg = "API error: " + doc["error"]["message"].as<String>();
            } else {
                errorMsg = "Unknown API error";
            }
            return "";
        }
        
        // Extract the response content
        if (!doc["content"].isNull()) {
            JsonArray contentArray = doc["content"];
            if (contentArray.size() > 0) {
                String responseText = "";
                
                // Iterate through content blocks
                for (JsonObject contentBlock : contentArray) {
                    if (contentBlock["type"] == "text") {
                        responseText += contentBlock["text"].as<String>();
                    }
                }
                
                // Extract stop reason if available
                if (!doc["stop_reason"].isNull()) {
                    // Return the exact stop_reason value without mapping
                    _lastFinishReason = doc["stop_reason"].as<String>();
                }
                
                // Extract token count if available
                if (!doc["usage"].isNull()) {
                    _lastTotalTokens = doc["usage"]["input_tokens"].as<int>() + 
                                      doc["usage"]["output_tokens"].as<int>();
                }
                
                return responseText;
            }
        }
        
        // If we get here, there was no valid content
        errorMsg = "No valid content in response";
        return "";
    } 
    catch (const std::exception& e) {
        errorMsg = "Exception during response parsing: " + String(e.what());
        return "";
    }
}

#ifdef ENABLE_TOOL_CALLS
// Build tool calls request body for Claude API
String AI_API_Claude_Handler::buildToolCallsRequestBody(const String& modelName,
                                                    const String* toolsArray, int toolsArraySize,
                                                    const String& systemMessage, const String& toolChoice,
                                                    int maxTokens,
                                                    const String& userMessage, JsonDocument& doc) {
    resetState();  // Reset finish reason and token count
    
    try {
        // Clear the document first to ensure no leftover fields
        doc.clear();
        
        // Set the model
        doc["model"] = modelName;
        
        // IMPORTANT: Claude API requires 'max_tokens' field - it cannot be omitted
        // According to Anthropic documentation: https://docs.anthropic.com/en/api/messages
        // The 'max_tokens' field is required and cannot be left empty
        // Use provided value if > 0, otherwise use default of 1024
        doc["max_tokens"] = (maxTokens > 0) ? maxTokens : 1024;
        
        // Add system message if specified (only if user has set it with setTCChatSystemRole)
        if (systemMessage.length() > 0) {
            doc["system"] = systemMessage;
        }
        
        // Create tools array
        JsonArray tools = doc["tools"].to<JsonArray>();
        
        // Add each tool to the tools array
        for (int i = 0; i < toolsArraySize; i++) {
            // Parse the tool definition from the input array
            JsonDocument toolDoc;
            DeserializationError error = deserializeJson(toolDoc, toolsArray[i]);
            
            if (error) {
                #ifdef ENABLE_DEBUG_OUTPUT
                Serial.println("Error parsing tool JSON: " + String(error.c_str()));
                #endif
                return "";
            }
            
            // Create a new tool object in Claude's format
            JsonObject tool = tools.add<JsonObject>();
            
            // Extract data from the library's tool format and convert to Claude's format
            if (!toolDoc["type"].isNull() && !toolDoc["function"].isNull()) {
                // OpenAI-style tool format: convert to Claude format
                JsonObject function = toolDoc["function"];
                
                // Add name and description
                tool["name"] = function["name"].as<String>();
                tool["description"] = function["description"].as<String>();
                
                // Add input schema
                JsonObject inputSchema = tool["input_schema"].to<JsonObject>();
                
                // Copy parameters object to input_schema
                if (!function["parameters"].isNull()) {
                    JsonObject params = function["parameters"];
                    
                    // Directly copy parameters
                    for (JsonPair kv : params) {
                        inputSchema[kv.key()] = kv.value();
                    }
                }
            } else {
                // Simpler format - copy directly
                tool["name"] = toolDoc["name"].as<String>();
                
                if (!toolDoc["description"].isNull()) {
                    tool["description"] = toolDoc["description"].as<String>();
                }
                
                // Add input schema
                JsonObject inputSchema = tool["input_schema"].to<JsonObject>();
                
                // Copy parameters object to input_schema
                if (!toolDoc["parameters"].isNull()) {
                    JsonObject params = toolDoc["parameters"];
                    
                    // Directly copy parameters
                    for (JsonPair kv : params) {
                        inputSchema[kv.key()] = kv.value();
                    }
                }
            }
        }
        
        // Create messages array with user message
        JsonArray messages = doc["messages"].to<JsonArray>();
        JsonObject userMsg = messages.add<JsonObject>();
        userMsg["role"] = "user";
        userMsg["content"] = userMessage;
        
        // Add tool_choice if specified by user with setTCChatToolChoice
        if (toolChoice.length() > 0) {
            String trimmedChoice = toolChoice;
            trimmedChoice.trim();
            
            // Check if it's one of the allowed string values
            if (trimmedChoice == "auto" || trimmedChoice == "any" || trimmedChoice == "none") {
                // For Claude, use object format with type field
                JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                toolChoiceObj["type"] = trimmedChoice;
            } 
            // Check if it starts with { - might be a JSON object string
            else if (trimmedChoice.startsWith("{")) {
                // Try to parse it as a JSON object
                JsonDocument toolChoiceDoc;
                DeserializationError error = deserializeJson(toolChoiceDoc, trimmedChoice);
                
                if (!error) {
                    // Successfully parsed as JSON - add as an object
                    JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                    
                    // Copy all fields from the parsed JSON
                    for (JsonPair kv : toolChoiceDoc.as<JsonObject>()) {
                        if (kv.value().is<JsonObject>()) {
                            JsonObject subObj = toolChoiceObj[kv.key().c_str()].to<JsonObject>();
                            JsonObject srcSubObj = kv.value().as<JsonObject>();
                            
                            for (JsonPair subKv : srcSubObj) {
                                subObj[subKv.key().c_str()] = subKv.value();
                            }
                        } else {
                            toolChoiceObj[kv.key().c_str()] = kv.value();
                        }
                    }
                } else {
                    // Not valid JSON - add as object with type field but this will likely cause an API error
                    #ifdef ENABLE_DEBUG_OUTPUT
                    Serial.println("Warning: tool_choice value is not valid JSON: " + trimmedChoice);
                    #endif
                    JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                    toolChoiceObj["type"] = trimmedChoice;
                }
            } else {
                // Not a recognized string value or JSON - add as object with type field but will likely cause an API error
                #ifdef ENABLE_DEBUG_OUTPUT
                Serial.println("Warning: tool_choice value is not recognized: " + trimmedChoice);
                #endif
                JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                toolChoiceObj["type"] = trimmedChoice;
            }
        }
        
        // Serialize the request body
        String requestBody;
        serializeJson(doc, requestBody);
        return requestBody;
    } 
    catch (const std::exception& e) {
        #ifdef ENABLE_DEBUG_OUTPUT
        Serial.println("Exception in buildToolCallsRequestBody: " + String(e.what()));
        #endif
        return "";
    }
}

// Parse tool calls response body from Claude API
String AI_API_Claude_Handler::parseToolCallsResponseBody(const String& responsePayload,
                                                    String& errorMsg, JsonDocument& doc) {
    resetState();  // Reset finish reason and token count
    
    try {
        // Parse the JSON response
        DeserializationError error = deserializeJson(doc, responsePayload);
        if (error) {
            errorMsg = "JSON parsing error: " + String(error.c_str());
            return "";
        }
        
        // Check for error in the response
        if (!doc["error"].isNull()) {
            if (!doc["error"]["message"].isNull()) {
                errorMsg = "API error: " + doc["error"]["message"].as<String>();
            } else {
                errorMsg = "Unknown API error";
            }
            return "";
        }
        
        // Extract token count if available
        if (!doc["usage"].isNull()) {
            _lastTotalTokens = doc["usage"]["input_tokens"].as<int>() + 
                              doc["usage"]["output_tokens"].as<int>();
        }
        
        // Extract the stop_reason (finish reason) without mapping
        if (!doc["stop_reason"].isNull()) {
            _lastFinishReason = doc["stop_reason"].as<String>();
        }
        
        // Check if the response contains content
        if (doc["content"].isNull() || !doc["content"].is<JsonArray>()) {
            errorMsg = "No content array found in response";
            return "";
        }
        
        JsonArray contentArray = doc["content"];
        
        // Check if there are any tool_use blocks in the content
        bool hasToolUse = false;
        for (JsonObject contentBlock : contentArray) {
            if (contentBlock["type"] == "tool_use") {
                hasToolUse = true;
                break;
            }
        }
        
        // If there are tool_use blocks, extract them into a JSON array
        if (hasToolUse) {
            // Create a new document to hold the tool calls array
            JsonDocument toolCallsDoc;
            JsonArray toolCalls = toolCallsDoc.to<JsonArray>();
            
            // Extract each tool_use block
            for (JsonObject contentBlock : contentArray) {
                if (contentBlock["type"] == "tool_use") {
                    // Create a new tool call object in OpenAI-compatible format
                    JsonObject toolCall = toolCalls.add<JsonObject>();
                    
                    // Set the ID
                    toolCall["id"] = contentBlock["id"];
                    
                    // Set the type to "function"
                    toolCall["type"] = "function";
                    
                    // Create the function object
                    JsonObject function = toolCall["function"].to<JsonObject>();
                    function["name"] = contentBlock["name"];
                    
                    // Convert input object to arguments string
                    if (contentBlock["input"].is<JsonObject>()) {
                        String argsStr;
                        serializeJson(contentBlock["input"], argsStr);
                        function["arguments"] = argsStr;
                    } else {
                        function["arguments"] = "{}";
                    }
                }
            }
            
            // Serialize the tool calls array to a string
            String toolCallsJson;
            serializeJson(toolCalls, toolCallsJson);
            
            #ifdef ENABLE_DEBUG_OUTPUT
            Serial.println("Tool calls detected: " + toolCallsJson);
            #endif
            
            return toolCallsJson;
        } 
        // If no tool_use blocks, extract the text content
        else {
            String responseText = "";
            
            // Iterate through content blocks
            for (JsonObject contentBlock : contentArray) {
                if (contentBlock["type"] == "text") {
                    responseText += contentBlock["text"].as<String>();
                }
            }
            
            return responseText;
        }
    } 
    catch (const std::exception& e) {
        errorMsg = "Exception during response parsing: " + String(e.what());
        
        #ifdef ENABLE_DEBUG_OUTPUT
        Serial.println("Exception in parseToolCallsResponseBody: " + String(e.what()));
        #endif
        
        return "";
    }
}

// Build follow-up request with tool results
String AI_API_Claude_Handler::buildToolCallsFollowUpRequestBody(const String& modelName,
                                                           const String* toolsArray, int toolsArraySize,
                                                           const String& systemMessage, const String& toolChoice,
                                                           const String& lastUserMessage,
                                                           const String& lastAssistantToolCallsJson,
                                                           const String& toolResultsJson,
                                                           int followUpMaxTokens,
                                                           const String& followUpToolChoice,
                                                           JsonDocument& doc) {
    resetState();  // Reset finish reason and token count
    
    try {
        // Clear the document first to ensure no leftover fields
        doc.clear();
        
        // Set the model
        doc["model"] = modelName;
        
        // IMPORTANT: Claude API requires 'max_tokens' field - it cannot be omitted
        // According to Anthropic documentation: https://docs.anthropic.com/en/api/messages
        // The 'max_tokens' field is required and cannot be left empty
        // Use provided value if > 0, otherwise use default of 1024
        doc["max_tokens"] = (followUpMaxTokens > 0) ? followUpMaxTokens : 1024;
        
        // Add system message if specified (only if user has set it with setTCChatSystemRole)
        if (systemMessage.length() > 0) {
            doc["system"] = systemMessage;
        }
        
        // Create tools array (same as in the original request)
        JsonArray tools = doc["tools"].to<JsonArray>();
        
        // Add each tool to the tools array
        for (int i = 0; i < toolsArraySize; i++) {
            // Parse the tool definition from the input array
            JsonDocument toolDoc;
            DeserializationError error = deserializeJson(toolDoc, toolsArray[i]);
            
            if (error) {
                #ifdef ENABLE_DEBUG_OUTPUT
                Serial.println("Error parsing tool JSON in follow-up: " + String(error.c_str()));
                #endif
                return "";
            }
            
            // Create a new tool object in Claude's format
            JsonObject tool = tools.add<JsonObject>();
            
            // Extract data from the library's tool format and convert to Claude's format
            if (!toolDoc["type"].isNull() && !toolDoc["function"].isNull()) {
                // OpenAI-style tool format: convert to Claude format
                JsonObject function = toolDoc["function"];
                
                // Add name and description
                tool["name"] = function["name"].as<String>();
                tool["description"] = function["description"].as<String>();
                
                // Add input schema
                JsonObject inputSchema = tool["input_schema"].to<JsonObject>();
                
                // Copy parameters object to input_schema
                if (!function["parameters"].isNull()) {
                    JsonObject params = function["parameters"];
                    
                    // Directly copy parameters
                    for (JsonPair kv : params) {
                        inputSchema[kv.key()] = kv.value();
                    }
                }
            } else {
                // Simpler format - copy directly
                tool["name"] = toolDoc["name"].as<String>();
                
                if (!toolDoc["description"].isNull()) {
                    tool["description"] = toolDoc["description"].as<String>();
                }
                
                // Add input schema
                JsonObject inputSchema = tool["input_schema"].to<JsonObject>();
                
                // Copy parameters object to input_schema
                if (!toolDoc["parameters"].isNull()) {
                    JsonObject params = toolDoc["parameters"];
                    
                    // Directly copy parameters
                    for (JsonPair kv : params) {
                        inputSchema[kv.key()] = kv.value();
                    }
                }
            }
        }
        
        // Create messages array
        JsonArray messages = doc["messages"].to<JsonArray>();
        
        // Add original user message
        JsonObject userMsg = messages.add<JsonObject>();
        userMsg["role"] = "user";
        userMsg["content"] = lastUserMessage;
        
        // Parse the assistant's response to extract the tool_use content
        JsonDocument assistantResponseDoc;
        DeserializationError assistantError = deserializeJson(assistantResponseDoc, lastAssistantToolCallsJson);
        
        if (assistantError) {
            #ifdef ENABLE_DEBUG_OUTPUT
            Serial.println("Error parsing assistant tool calls: " + String(assistantError.c_str()));
            #endif
            return ""; // Error parsing assistant's tool calls
        }
        
        // Add assistant's response as a message
        JsonObject assistantMsg = messages.add<JsonObject>();
        assistantMsg["role"] = "assistant";
        
        // Create content array for assistant message
        JsonArray assistantContent = assistantMsg["content"].to<JsonArray>();
        
        // Check if the assistant response is already in Claude's format
        // (it might be the direct response from Claude with content array)
        if (assistantResponseDoc["content"].is<JsonArray>()) {
            // Copy the entire content array from the original response
            JsonArray originalContent = assistantResponseDoc["content"];
            for (size_t i = 0; i < originalContent.size(); i++) {
                // Deep copy each element in the content array
                assistantContent.add(originalContent[i]);
            }
        } 
        // If it's in our library's format (array of tool calls)
        else if (assistantResponseDoc.is<JsonArray>()) {
            // First add a placeholder text element (required by Claude)
            JsonObject textBlock = assistantContent.add<JsonObject>();
            textBlock["type"] = "text";
            textBlock["text"] = "I'll help you with that.";
            
            // Then add the tool_use blocks
            JsonArray toolCalls = assistantResponseDoc.as<JsonArray>();
            for (JsonObject toolCall : toolCalls) {
                JsonObject toolUseBlock = assistantContent.add<JsonObject>();
                toolUseBlock["type"] = "tool_use";
                toolUseBlock["id"] = toolCall["id"].as<String>();
                toolUseBlock["name"] = toolCall["function"]["name"].as<String>();
                
                // Handle input parameters
                JsonObject inputObj = toolUseBlock["input"].to<JsonObject>();
                // Parse arguments string to object
                String argsStr = toolCall["function"]["arguments"].as<String>();
                JsonDocument argsDoc;
                DeserializationError argsError = deserializeJson(argsDoc, argsStr);
                if (!argsError) {
                    // Copy arguments to input
                    for (JsonPair kv : argsDoc.as<JsonObject>()) {
                        inputObj[kv.key()] = kv.value();
                    }
                } else {
                    #ifdef ENABLE_DEBUG_OUTPUT
                    Serial.println("Error parsing tool arguments: " + String(argsError.c_str()));
                    #endif
                }
            }
        }
        
        // Add user's tool result message according to Claude's format
        JsonObject toolResultMsg = messages.add<JsonObject>();
        toolResultMsg["role"] = "user";
        
        // Create content array for tool result message
        JsonArray toolResultContent = toolResultMsg["content"].to<JsonArray>();
        
        // Parse the tool results JSON and format for Claude
        JsonDocument resultsDoc;
        DeserializationError resultsError = deserializeJson(resultsDoc, toolResultsJson);
        
        if (resultsError) {
            #ifdef ENABLE_DEBUG_OUTPUT
            Serial.println("Error parsing tool results: " + String(resultsError.c_str()));
            #endif
            return ""; // Error parsing tool results
        }
        
        // Process each tool result following Claude's format
        JsonArray resultsArray = resultsDoc.as<JsonArray>();
        for (JsonObject result : resultsArray) {
            // Create tool_result content block
            JsonObject toolResultBlock = toolResultContent.add<JsonObject>();
            toolResultBlock["type"] = "tool_result";
            
            // Set the tool_use_id from tool_call_id
            if (!result["tool_call_id"].isNull()) {
                toolResultBlock["tool_use_id"] = result["tool_call_id"].as<String>();
            } else {
                #ifdef ENABLE_DEBUG_OUTPUT
                Serial.println("Warning: tool_call_id missing in tool result");
                #endif
                continue; // Skip this result if no tool_call_id
            }
            
            // Handle function output - for Claude we need to send the content directly
            if (result["function"].is<JsonObject>() && !result["function"]["output"].isNull()) {
                String output = result["function"]["output"].as<String>();
                
                // Check if output is a JSON string by looking for { at the beginning
                if (output.startsWith("{")) {
                    // Try to parse as JSON to see if it's valid
                    JsonDocument outputDoc;
                    DeserializationError outputError = deserializeJson(outputDoc, output);
                    
                    if (!outputError) {
                        // It's valid JSON, directly assign as content
                        toolResultBlock["content"] = output;
                    } else {
                        // Not valid JSON, just use as plain text
                        toolResultBlock["content"] = output;
                    }
                } else {
                    // Plain text output
                    toolResultBlock["content"] = output;
                }
            }
            
            // Add is_error flag if present
            if (!result["is_error"].isNull() && result["is_error"].as<bool>()) {
                toolResultBlock["is_error"] = true;
            }
        }
        
        // Add tool_choice if specified for the follow-up by user with setTCReplyToolChoice
        if (followUpToolChoice.length() > 0) {
            String trimmedChoice = followUpToolChoice;
            trimmedChoice.trim();
            
            // Check if it's one of the allowed string values
            if (trimmedChoice == "auto" || trimmedChoice == "any" || trimmedChoice == "none") {
                // For Claude, use object format with type field
                JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                toolChoiceObj["type"] = trimmedChoice;
            } 
            // Check if it starts with { - might be a JSON object string
            else if (trimmedChoice.startsWith("{")) {
                // Try to parse it as a JSON object
                JsonDocument toolChoiceDoc;
                DeserializationError error = deserializeJson(toolChoiceDoc, trimmedChoice);
                
                if (!error) {
                    // Successfully parsed as JSON - add as an object
                    JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                    
                    // Copy all fields from the parsed JSON
                    for (JsonPair kv : toolChoiceDoc.as<JsonObject>()) {
                        if (kv.value().is<JsonObject>()) {
                            JsonObject subObj = toolChoiceObj[kv.key().c_str()].to<JsonObject>();
                            JsonObject srcSubObj = kv.value().as<JsonObject>();
                            
                            for (JsonPair subKv : srcSubObj) {
                                subObj[subKv.key().c_str()] = subKv.value();
                            }
                        } else {
                            toolChoiceObj[kv.key().c_str()] = kv.value();
                        }
                    }
                } else {
                    // Not valid JSON - add as object with type field but this will likely cause an API error
                    #ifdef ENABLE_DEBUG_OUTPUT
                    Serial.println("Warning: tool_choice value is not valid JSON: " + trimmedChoice);
                    #endif
                    JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                    toolChoiceObj["type"] = trimmedChoice;
                }
            } else {
                // Not a recognized string value or JSON - add as object with type field but will likely cause an API error
                #ifdef ENABLE_DEBUG_OUTPUT
                Serial.println("Warning: tool_choice value is not recognized: " + trimmedChoice);
                #endif
                JsonObject toolChoiceObj = doc["tool_choice"].to<JsonObject>();
                toolChoiceObj["type"] = trimmedChoice;
            }
        }
        
        // Serialize the request body
        String requestBody;
        serializeJson(doc, requestBody);
        
        #ifdef ENABLE_DEBUG_OUTPUT
        Serial.println("---------- Claude Tool Calls Follow-up Request ----------");
        Serial.println(requestBody);
        Serial.println("----------------------------------------------------------");
        #endif
        
        return requestBody;
    } 
    catch (const std::exception& e) {
        #ifdef ENABLE_DEBUG_OUTPUT
        Serial.println("Exception in buildToolCallsFollowUpRequestBody: " + String(e.what()));
        #endif
        return "";
    }
}
#endif // ENABLE_TOOL_CALLS

#ifdef ENABLE_STREAM_CHAT
String AI_API_Claude_Handler::buildStreamRequestBody(const String& modelName, const String& systemRole,
                                                    float temperature, int maxTokens,
                                                    const String& userMessage, JsonDocument& doc,
                                                    const String& customParams) {
    try {
        // Use the same logic as buildRequestBody but add "stream": true
        
        // Set the model
        doc["model"] = modelName;
        
        // Enable streaming
        doc["stream"] = true;
        
        // Process custom parameters if provided
        if (customParams.length() > 0) {
            // Create a temporary document to parse the custom parameters
            JsonDocument paramsDoc;
            DeserializationError error = deserializeJson(paramsDoc, customParams);
            
            // Only proceed if parsing was successful
            if (!error) {
                // Add each parameter from customParams to the request (skip conflicting ones)
                for (JsonPair param : paramsDoc.as<JsonObject>()) {
                    // Skip model, messages, system, stream as they are handled separately
                    if (param.key() != "model" && param.key() != "messages" && 
                        param.key() != "system" && param.key() != "stream") {
                        doc[param.key()] = param.value();
                    }
                }
            }
        }
        
        // Add optional parameters (these override any custom parameters)
        if (temperature >= 0) {
            doc["temperature"] = temperature;
        }
        
        // IMPORTANT: Claude API requires 'max_tokens' field - it cannot be omitted
        // According to Anthropic documentation: https://docs.anthropic.com/en/api/messages
        // The 'max_tokens' field is required and cannot be left empty
        // Use provided value if > 0, otherwise use default of 1024
        doc["max_tokens"] = (maxTokens > 0) ? maxTokens : 1024;
        
        // Add system message if specified
        if (systemRole.length() > 0) {
            doc["system"] = systemRole;
        }
        
        // Create messages array with user message
        JsonArray messages = doc["messages"].to<JsonArray>();
        JsonObject userMsg = messages.add<JsonObject>();
        userMsg["role"] = "user";
        userMsg["content"] = userMessage;
        
        // Serialize the request body
        String requestBody;
        serializeJson(doc, requestBody);
        return requestBody;
    } 
    catch (const std::exception& e) {
        return "";
    }
}

String AI_API_Claude_Handler::processStreamChunk(const String& rawChunk, bool& isComplete, String& errorMsg) {
    resetState(); // Reset state for each chunk
    isComplete = false;
    errorMsg = "";

    // Claude streaming uses Server-Sent Events format
    // Format: "event: event_type\ndata: {json}\n" or just "data: {json}\n"
    
    if (rawChunk.isEmpty()) {
        return "";
    }

    // Look for "data: " prefix (standard SSE format)
    int dataIndex = rawChunk.indexOf("data: ");
    if (dataIndex == -1) {
        // Not a data line, might be event line or empty line, skip
        return "";
    }

    // Extract JSON part after "data: "
    String jsonPart = rawChunk.substring(dataIndex + 6); // 6 = length of "data: "
    jsonPart.trim(); // Remove any whitespace

    if (jsonPart.isEmpty()) {
        return "";
    }

    // Parse the JSON chunk
    JsonDocument chunkDoc; // Larger buffer for Claude responses
    DeserializationError error = deserializeJson(chunkDoc, jsonPart);
    if (error) {
        errorMsg = "Failed to parse Claude streaming chunk JSON: " + String(error.c_str());
        return "";
    }

    // Check for error in the chunk
    if (!chunkDoc["error"].isNull()) {
        errorMsg = String("API Error in stream: ") + (chunkDoc["error"]["message"] | "Unknown error");
        return "";
    }

    // Extract the event type
    String eventType = "";
    if (!chunkDoc["type"].isNull()) {
        eventType = chunkDoc["type"].as<String>();
    }

    // Handle different event types according to Claude's streaming documentation
    if (eventType == "message_start") {
        // Beginning of message - no content yet
        return "";
    }
    else if (eventType == "content_block_start") {
        // Start of a content block - no content yet
        return "";
    }
    else if (eventType == "content_block_delta") {
        // This contains the actual content chunks we want
        if (!chunkDoc["delta"].isNull()) {
            JsonObject delta = chunkDoc["delta"];
            
            // Check if this is a text delta
            if (delta["type"] == "text_delta") {
                if (!delta["text"].isNull()) {
                    return delta["text"].as<String>();
                }
            }
        }
        return "";
    }
    else if (eventType == "content_block_stop") {
        // End of a content block - no content
        return "";
    }
    else if (eventType == "message_delta") {
        // Message-level changes, check for stop_reason
        if (!chunkDoc["delta"].isNull()) {
            JsonObject delta = chunkDoc["delta"];
            if (!delta["stop_reason"].isNull()) {
                _lastFinishReason = delta["stop_reason"].as<String>();
            }
        }
        return "";
    }
    else if (eventType == "message_stop") {
        // End of message
        isComplete = true;
        return "";
    }
    else if (eventType == "ping") {
        // Keep-alive ping - ignore
        return "";
    }
    else if (eventType == "error") {
        // Error event
        if (!chunkDoc["error"].isNull()) {
            JsonObject errorObj = chunkDoc["error"];
            errorMsg = String("Stream error: ") + (errorObj["message"] | "Unknown error");
        } else {
            errorMsg = "Unknown stream error";
        }
        return "";
    }

    // If we reach here, it might be an unrecognized event type
    // Just ignore it and continue
    return "";
}
#endif // ENABLE_STREAM_CHAT

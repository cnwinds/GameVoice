protoc message.proto --cpp_out=../NetVoice/proto
@if %ERRORLEVEL% neq 0 pause

protoc message.proto --go_out=../VoiceServer/src/message
@if %ERRORLEVEL% neq 0 pause
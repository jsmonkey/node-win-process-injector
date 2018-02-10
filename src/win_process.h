#ifndef SRC_WIN_PROCESS_H_
#define SRC_WIN_PROCESS_H_

#include <uv.h>
#include <node.h>
#include <node_object_wrap.h>

namespace injector {

class WinProcess: public node::ObjectWrap {

    public:
        static void Init(v8::Local<v8::Object> exports);
        void* GetHandler();
        void SetHandler(void* handler);
        unsigned int GetProcessId();
    
    private:
        explicit WinProcess(unsigned int process_id = 0);
        ~WinProcess();

        void* handler;
        unsigned int process_id;

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void OpenHandler(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void ReadMemoryAt(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void AllocateMemory(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void FreeMemoryAt(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void WriteToMemoryAt(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Inject(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Terminate(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void CloseHandler(const v8::FunctionCallbackInfo<v8::Value>& args);
        static v8::Persistent<v8::Function> constructor;
};

}

#endif
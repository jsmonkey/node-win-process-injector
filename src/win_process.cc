#include "win_process.h"
#include "process_manager.h"

#include <node_buffer.h>

namespace injector {

struct WorkerSharedData {
    uv_work_t request;
    v8::Persistent<v8::Promise::Resolver> promise_resolver;
};

struct ProcessData: WorkerSharedData {
    std::string process_name;
    unsigned int process_id;
    WinProcess* win_process;
};

struct MemoryData: WorkerSharedData {
    bool is_action_successful;
    void* address;
    unsigned int length;
    unsigned char* buffer;
    WinProcess* win_process;
    unsigned int error;
};

v8::Persistent<v8::Function> WinProcess::constructor;

void test(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    void* address = (void*)args[0]->IntegerValue();
    args.GetReturnValue().Set(v8::Number::New(isolate, reinterpret_cast<unsigned int>(address)));
}

void GetProcessIdByName(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    v8::String::Utf8Value node_process_name (args[0]->ToString());
    std::string process_name = std::string(*node_process_name);

    ProcessData* data = new ProcessData();
    data->request.data = data;
    data->promise_resolver.Reset(isolate, persistent_resolver);

    data->process_name = process_name;

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        ProcessData* data = static_cast<ProcessData*>(request->data);
        data->process_id = ProcessManager::GetPIDByName(data->process_name);
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);
        ProcessData* data = static_cast<ProcessData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->process_id != NULL) {
            resolver->Resolve(v8::Number::New(isolate, data->process_id));
        } else {
            resolver->Reject(v8::String::NewFromUtf8(isolate, "Error: Process not found."));
        }
        data->promise_resolver.Reset();
        delete data;
    });

    args.GetReturnValue().Set(promise);
};

WinProcess::WinProcess(unsigned int process_id) {
    this->process_id = process_id;
    this->handler = NULL;
};

WinProcess::~WinProcess() {
};

void* WinProcess::GetHandler() {
    return this->handler;
};

void WinProcess::SetHandler(void* handler) {
    this->handler = handler;
};

unsigned int WinProcess::GetProcessId() {
    return this->process_id;
};

void WinProcess::Init(v8::Local<v8::Object> exports) {
    v8::Isolate* isolate = exports->GetIsolate();

    v8::Local<v8::FunctionTemplate> function_template = v8::FunctionTemplate::New(isolate, WinProcess::New);
    function_template->SetClassName(v8::String::NewFromUtf8(isolate, "WinProcess"));
    function_template->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(function_template, "openHandler", WinProcess::OpenHandler);
    NODE_SET_PROTOTYPE_METHOD(function_template, "closeHandler", WinProcess::CloseHandler);
    NODE_SET_PROTOTYPE_METHOD(function_template, "readMemoryAt", WinProcess::ReadMemoryAt);
    NODE_SET_PROTOTYPE_METHOD(function_template, "allocateMemory", WinProcess::AllocateMemory);
    NODE_SET_PROTOTYPE_METHOD(function_template, "freeMemoryAt", WinProcess::FreeMemoryAt);
    NODE_SET_PROTOTYPE_METHOD(function_template, "writeToMemoryAt", WinProcess::WriteToMemoryAt);
    NODE_SET_PROTOTYPE_METHOD(function_template, "inject", WinProcess::Inject);

    constructor.Reset(isolate, function_template->GetFunction());
    exports->Set(v8::String::NewFromUtf8(isolate, "WinProcess"), function_template->GetFunction());


    NODE_SET_METHOD(exports, "getProcessIdByName", GetProcessIdByName);
    NODE_SET_METHOD(exports, "test", test);
};

void WinProcess::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    if(args.IsConstructCall()) {
        const int process_id = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
        WinProcess* process = new WinProcess(process_id);
        process->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
    } else {
        const int argc = 1;
        v8::Local<v8::Value> argv[argc] = { args[0] };
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Function> wraped_constructor = v8::Local<v8::Function>::New(isolate, constructor);
        v8::Local<v8::Object> instance = wraped_constructor->NewInstance(context, argc, argv).ToLocalChecked();
        args.GetReturnValue().Set(instance);
    }
};

void WinProcess::OpenHandler(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    ProcessData* data = new ProcessData();
    data->request.data = data;
    data->win_process = node::ObjectWrap::Unwrap<WinProcess>(args.Holder());
    data->promise_resolver.Reset(isolate, persistent_resolver);

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        ProcessData* data = static_cast<ProcessData*>(request->data);
        data->win_process->SetHandler(OpenProcess(PROCESS_ALL_ACCESS, false, data->win_process->GetProcessId()));
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        ProcessData* data = static_cast<ProcessData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->win_process->GetHandler() != NULL) {
            resolver->Resolve(v8::Undefined(isolate));
        } else {
            resolver->Reject(v8::Number::New(isolate, GetLastError()));
        }
        data->promise_resolver.Reset();
        delete data;
    });

    args.GetReturnValue().Set(promise);
};

void WinProcess::ReadMemoryAt(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Two arguments required")));
        return;
    }

    if(!args[0]->IsNumber() || !args[1]->IsNumber()) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Arguments must be of Number type")));
        return;
    }

    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    MemoryData* data = new MemoryData();
    data->request.data = data;
    data->win_process = node::ObjectWrap::Unwrap<WinProcess>(args.Holder());
    data->promise_resolver.Reset(isolate, persistent_resolver);

    data->length = (unsigned int)args[1]->IntegerValue();
    data->address = (void*)args[0]->IntegerValue();
    data->buffer = new unsigned char[data->length];

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        size_t bytes_read;
        MemoryData* data = static_cast<MemoryData*>(request->data);
        data->is_action_successful = ReadProcessMemory(data->win_process->GetHandler(), data->address, data->buffer, data->length, &bytes_read);
        data->error = GetLastError();
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        MemoryData* data = static_cast<MemoryData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->is_action_successful) {
            resolver->Resolve(node::Buffer::Copy(isolate, (char*)data->buffer, (size_t)data->length).ToLocalChecked());
        } else {
            resolver->Reject(v8::Number::New(isolate, data->error));
        }
        data->promise_resolver.Reset();
        delete[] data->buffer;
        delete data;
    });

    args.GetReturnValue().Set(promise);
};

void WinProcess::AllocateMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "At least one argument required")));
        return;
    }

    if(!args[0]->IsNumber()) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Argument must be of Number type")));
        return;
    }

    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    MemoryData* data = new MemoryData();
    data->request.data = data;
    data->win_process = node::ObjectWrap::Unwrap<WinProcess>(args.Holder());
    data->promise_resolver.Reset(isolate, persistent_resolver);

    data->length = args[0]->IntegerValue();

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        MemoryData* data = static_cast<MemoryData*>(request->data);
        data->address = VirtualAllocEx(data->win_process->GetHandler(), NULL, (size_t)data->length, MEM_COMMIT, PAGE_READWRITE);
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        MemoryData* data = static_cast<MemoryData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->address != NULL) {
            resolver->Resolve(node::Buffer::Copy(isolate, (char*)&data->address, sizeof(data->address)).ToLocalChecked());
        } else {
            resolver->Reject(v8::Number::New(isolate, GetLastError()));
        }
        data->promise_resolver.Reset();
        delete data;
    });

    args.GetReturnValue().Set(promise);
}

void WinProcess::FreeMemoryAt(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "At least one argument required")));
        return;
    }

    if(!args[0]->IsNumber()) {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Argument must be of Number type")));
        return;
    }

    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    MemoryData* data = new MemoryData();
    data->request.data = data;
    data->win_process = node::ObjectWrap::Unwrap<WinProcess>(args.Holder());
    data->promise_resolver.Reset(isolate, persistent_resolver);

    data->address = (void*)args[0]->IntegerValue();

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        MemoryData* data = static_cast<MemoryData*>(request->data);
        data->is_action_successful = VirtualFreeEx(data->win_process->GetHandler(), data->address, 0, MEM_RELEASE);
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        MemoryData* data = static_cast<MemoryData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->is_action_successful) {
            resolver->Resolve(v8::Boolean::New(isolate, true));
        } else {
            resolver->Reject(v8::Number::New(isolate, GetLastError()));
        }
        data->promise_resolver.Reset();
        delete data;
    });

    args.GetReturnValue().Set(promise);
}

void WinProcess::WriteToMemoryAt(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    MemoryData* data = new MemoryData();
    data->request.data = data;
    data->win_process = node::ObjectWrap::Unwrap<WinProcess>(args.Holder());
    data->promise_resolver.Reset(isolate, persistent_resolver);

    data->address = (void*)args[0]->IntegerValue();
    data->buffer = (unsigned char*)node::Buffer::Data(args[1]);
    data->length = (unsigned int)node::Buffer::Length(args[1]);

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        MemoryData* data = static_cast<MemoryData*>(request->data);
        data->is_action_successful = WriteProcessMemory(data->win_process->GetHandler(), data->address, (void*)data->buffer, (size_t)data->length, NULL);
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        MemoryData* data = static_cast<MemoryData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->is_action_successful) {
            resolver->Resolve(v8::Boolean::New(isolate, true));
        } else {
            resolver->Reject(v8::Number::New(isolate, GetLastError()));
        }
        data->promise_resolver.Reset();
        delete data;
    });

    args.GetReturnValue().Set(promise);
};

void WinProcess::Inject(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    
    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    MemoryData* data = new MemoryData();
    data->request.data = data;
    data->win_process = node::ObjectWrap::Unwrap<WinProcess>(args.Holder());
    data->promise_resolver.Reset(isolate, persistent_resolver);

    data->buffer = (unsigned char*)node::Buffer::Data(args[0]);
    data->length = (unsigned int)node::Buffer::Length(args[0]);

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        MemoryData* data = static_cast<MemoryData*>(request->data);
        data->is_action_successful = ProcessManager::Inject(data->win_process->GetHandler(), (void*)data->buffer, (size_t)data->length, NULL, NULL);
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        MemoryData* data = static_cast<MemoryData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->is_action_successful) {
            resolver->Resolve(v8::Boolean::New(isolate, true));
        } else {
            resolver->Reject(v8::Number::New(isolate, GetLastError()));
        }
        data->promise_resolver.Reset();
        delete data;
    });

    args.GetReturnValue().Set(promise);
};

void WinProcess::CloseHandler(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    
    v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate);
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Persistent<v8::Promise::Resolver> persistent_resolver(isolate, resolver);

    MemoryData* data = new MemoryData();
    data->request.data = data;
    data->win_process = node::ObjectWrap::Unwrap<WinProcess>(args.Holder());
    data->promise_resolver.Reset(isolate, persistent_resolver);

    uv_queue_work(uv_default_loop(), &data->request, 
    [](uv_work_t* request) {
        MemoryData* data = static_cast<MemoryData*>(request->data);
        data->is_action_successful = CloseHandle(data->win_process->GetHandler());
    }, 
    [](uv_work_t* request, int status) {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        MemoryData* data = static_cast<MemoryData*>(request->data);
        v8::Local<v8::Promise::Resolver> resolver = v8::Local<v8::Promise::Resolver>::New(isolate, data->promise_resolver);
        if(data->is_action_successful) {
            resolver->Resolve(v8::Boolean::New(isolate, true));
        } else {
            resolver->Reject(v8::Number::New(isolate, GetLastError()));
        }
        data->promise_resolver.Reset();
        delete data;
    });

    args.GetReturnValue().Set(promise);
};

}


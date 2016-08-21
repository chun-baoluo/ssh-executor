#include <libssh2.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include <node.h>
#include <node_buffer.h>
#include <v8.h>

using namespace v8;

bool connected = false;
int sock;
int rc;
LIBSSH2_SESSION *session;
LIBSSH2_CHANNEL *channel;
LIBSSH2_KNOWNHOSTS *nh;

static int waitsocket(int socket_fd, LIBSSH2_SESSION *session) {
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&fd);

    FD_SET(socket_fd, &fd);

    dir = libssh2_session_block_directions(session);

    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;

    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;

    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

    return rc;
}

void connect(const FunctionCallbackInfo<Value>& args) {
    struct sockaddr_in sin;
    unsigned long hostaddr;
    char *error = "";

	Isolate* isolate = args.GetIsolate();

    Handle<Object> obj = Handle<Object>::Cast(args[0]);

    String::Utf8Value host(obj->Get(String::NewFromUtf8(isolate, "host")));
    String::Utf8Value username(obj->Get(String::NewFromUtf8(isolate, "username")));
    String::Utf8Value password(obj->Get(String::NewFromUtf8(isolate, "password")));
    
    int port = obj->Get(String::NewFromUtf8(isolate, "port"))->NumberValue();

    hostaddr = inet_addr(*host);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = hostaddr;
    if(connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        error = "Failed to connect.";
    } else {
        session = libssh2_session_init();

        libssh2_session_set_blocking(session, 0);

        while((rc = libssh2_session_handshake(session, sock)) == LIBSSH2_ERROR_EAGAIN);
        if (!rc) {
            nh = libssh2_knownhost_init(session);
            while ((rc = libssh2_userauth_password(session, *username, *password)) == LIBSSH2_ERROR_EAGAIN);
        }
    }

    // Callback
    Local<Function> cb = Local<Function>::Cast(args[1]);
    Local<Value> argv[1] = { String::NewFromUtf8(isolate, error) };
    if(error == "") {
        argv[0] = Null(isolate);
        connected = true;

    }
    cb->Call(Null(isolate), 1, argv);
}

void exec(const FunctionCallbackInfo<Value>& args) {
    char *error = "";
    char buffer[0x4000];
    Isolate* isolate = args.GetIsolate();
    
    if(connected == true) {

        v8::String::Utf8Value command(args[0]->ToString());

        while((channel = libssh2_channel_open_session(session)) == NULL && libssh2_session_last_error(session,NULL,NULL,0) == LIBSSH2_ERROR_EAGAIN) {
            waitsocket(sock, session);
        }

        while((rc = libssh2_channel_exec(channel, *command)) == LIBSSH2_ERROR_EAGAIN) {
            waitsocket(sock, session);
        }
        
        if( rc != 0 ) {
            error = "Error while executing command.";
        } else {
            for( ;; ) {
                int rc;
                
                do {
                    rc = libssh2_channel_read(channel, buffer, sizeof(buffer));
                } while( rc > 0 );

                if(rc == LIBSSH2_ERROR_EAGAIN) {
                    waitsocket(sock, session);
                } else {
                    break;
                }
            }           
        }
    }

    // removing new string symbol
    std::string result(buffer);
    result = result.substr(0, result.size() - 1);

    // Callback
    Local<Function> cb = Local<Function>::Cast(args[1]);
    Local<Value> argv[2] = {String::NewFromUtf8(isolate, result.c_str()), Null(isolate)};
    if(error != "") {
        argv[0] = Null(isolate);
        argv[1] = String::NewFromUtf8(isolate, error);
    }
    
    cb->Call(Null(isolate), 2, argv);
}

void close(const FunctionCallbackInfo<Value>& args) {
    if(connected == true) {
        static char *exitsignal=(char *)"none";
        while( (rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN )
            waitsocket(sock, session);

        if( rc == 0 ) {
            libssh2_channel_get_exit_signal(channel, &exitsignal, NULL, NULL, NULL, NULL, NULL);
        }

        libssh2_channel_free(channel);

        channel = NULL;

        libssh2_session_disconnect(session, "");
        libssh2_session_free(session);

        close(sock);

        libssh2_exit();
    
        connected = false;        
    }
}

void RegisterModule(Local<Object> exports) {
	NODE_SET_METHOD(exports, "connect", connect);
    NODE_SET_METHOD(exports, "exec", exec);
    NODE_SET_METHOD(exports, "close", close);
}

NODE_MODULE(modulename, RegisterModule);
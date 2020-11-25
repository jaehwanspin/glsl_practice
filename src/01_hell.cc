#include <iostream>
#include <string>
#include <memory>
#include <memory.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <GLES2/gl2platform.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
// #include <GLES3/gl3platform.h>
// #include <GLES3/gl31.h>
// #include <GLES3/gl3ext.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglmesaext.h>

#include <boost/gil/extension/io/png.hpp>

#define GLSL_STRINGIZE_SOURCE(version_number, ...) \
"#version "#version_number"\n" \
#__VA_ARGS__

namespace gl
{

class context
{
    Display*    x_display;
    Window      win;
    EGLDisplay  egl_display;
    EGLContext  egl_context;
    EGLSurface  egl_surface;

public:
    context()
    {
        this->init_x();
        this->init_egl();
    }
    ~context()
    {
        eglDestroyContext ( egl_display, egl_context );
        eglDestroySurface ( egl_display, egl_surface );
        eglTerminate      ( egl_display );
        XDestroyWindow    ( x_display, win );
        XCloseDisplay     ( x_display );
    }

private:
    void init_x()
    {
        this->x_display = XOpenDisplay(nullptr);
        if (this->x_display == nullptr)
        {
            std::cerr << "cannot connect to X server" << std::endl;
            return;
        }
        Window root = DefaultRootWindow(this->x_display);
        XSetWindowAttributes  swa;
        swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;
        win = XCreateWindow(   // create a window with the provided parameters
            x_display, root,
            0, 0, 1920, 1080, 0,
            CopyFromParent, InputOutput,
            CopyFromParent, CWEventMask,
            &swa);
        // XSetWindowAttributes  xattr;
        // Atom atom;
        // int one = 1;
        // xattr.override_redirect = False;
        // XChangeWindowAttributes ( x_display, win, CWOverrideRedirect, &xattr );
        // atom = XInternAtom ( x_display, "_NET_WM_STATE_FULLSCREEN", True );
        // XChangeProperty(
        //     x_display, win,
        //     XInternAtom (x_display, "_NET_WM_STATE", True),
        //     XA_ATOM, 32, PropModeReplace,
        //     (unsigned char*) &atom,  1 );
        
        // XChangeProperty(
        //     x_display, win,
        //     XInternAtom(x_display, "_HILDON_NON_COMPOSITED_WINDOW", False),
        //     XA_INTEGER,  32,  PropModeReplace,
        //     (unsigned char*)&one,  1);
        
        // XWMHints hints;
        // hints.input = True;
        // hints.flags = InputHint;
        // XSetWMHints(x_display, win, &hints);

        // XMapWindow ( x_display , win );             // make the window visible on the screen
        // XStoreName(x_display, win, "GL test");      // give the window a name

        // //// get identifiers for the provided atom name strings
        // Atom wm_state = XInternAtom(x_display, "_NET_WM_STATE", False);
        // Atom fullscreen = XInternAtom(x_display, "_NET_WM_STATE_FULLSCREEN", False);

        // XEvent xev;
        // memset(&xev, 0, sizeof(xev));

        // xev.type = ClientMessage;
        // xev.xclient.window = win;
        // xev.xclient.message_type = wm_state;
        // xev.xclient.format = 32;
        // xev.xclient.data.l[0] = 1;
        // xev.xclient.data.l[1] = fullscreen;
        // XSendEvent( // send an event mask to the X-server
        //     this->x_display,
        //     DefaultRootWindow(this->x_display),
        //     False,
        //     SubstructureNotifyMask,
        //     &xev);
    }

    void init_egl()
    {
        // egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        egl_display  =  eglGetDisplay( (EGLNativeDisplayType) x_display );
        if ( egl_display == EGL_NO_DISPLAY ) {
            std::cerr << "Got no EGL display." << std::endl;
            return;
        }
        
        if ( !eglInitialize( egl_display, NULL, NULL ) ) {
            std::cerr << "Unable to initialize EGL" << std::endl;
            return;
        }
        
        EGLint attr[] = {       // some attributes to set up our egl-interface
            EGL_BUFFER_SIZE, 16,
            EGL_RENDERABLE_TYPE,
            EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        
        EGLConfig  ecfg;
        EGLint     num_config;
        if ( !eglChooseConfig( egl_display, attr, &ecfg, 1, &num_config ) ) {
            std::cerr << "Failed to choose config (eglError: " << eglGetError() << ")" << std::endl;
            return;
        }
        
        if ( num_config != 1 ) {
            std::cerr << "Didn't get exactly one config, but " << num_config << std::endl;
            return;
        }
        
        egl_surface = eglCreateWindowSurface ( egl_display, ecfg, win, NULL );
        if ( egl_surface == EGL_NO_SURFACE ) {
            std::cerr << "Unable to create EGL surface (eglError: " << eglGetError() << ")" << std::endl;
            return;
        }
        
        //// egl-contexts collect all state descriptions needed required for operation
        EGLint ctxattr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        egl_context = eglCreateContext ( egl_display, ecfg, EGL_NO_CONTEXT, ctxattr );
        if ( egl_context == EGL_NO_CONTEXT ) {
            std::cerr << "Unable to create EGL context (eglError: " << eglGetError() << ")" << std::endl;
            return;
        }
        
        //// associate the egl-context with the egl-surface
        eglMakeCurrent( egl_display, egl_surface, egl_surface, egl_context );
            }
};

template<GLenum __shader_type>
class _shader_base
{
private:
    GLuint _shader;

public:
    _shader_base(std::string& source_code)
    {
        this->init(source_code.c_str());
    }
    _shader_base(const char* source_code)
    {
        this->init(source_code);
    }

    virtual ~_shader_base()
    {
        if (this->_shader)
            glDeleteShader(this->_shader);
    }

    bool compile()
    {
        GLint compiled;
        glCompileShader(this->_shader);
        glGetShaderiv(this->_shader, GL_COMPILE_STATUS, &compiled);
        return static_cast<bool>(compiled);
    }

    std::string info()
    {
        GLint len_info = 0;
        std::string info_log_str;
        glGetShaderiv(this->_shader, GL_INFO_LOG_LENGTH, &len_info);
        if (len_info > 1)
        {
            char *info_log = (char*)malloc(sizeof(char) * len_info);
            glGetShaderInfoLog(this->_shader, len_info, NULL, info_log);
            info_log_str = info_log;
            std::cout << info_log << std::endl;
            free(info_log);
        }

        return std::move(info_log_str);
    }

    GLint c_shader() { return this->_shader; }

private:
    void init(const char* source_code)
    {
        this->_shader = glCreateShader(__shader_type);
        if (this->_shader == 0) return;
        glShaderSource(this->_shader, 1, &source_code, nullptr);
    }

};
typedef _shader_base<GL_VERTEX_SHADER> vertex_shader;
typedef _shader_base<GL_FRAGMENT_SHADER> fragment_shader;
// typedef _shader_base<GL_COMPUTE_SHADER> compute_shader;

class program
{
    GLuint _program;

public:
    program()
    {
        this->_program = glCreateProgram();
    }
    ~program()
    {
        if (this->_program)
            glDeleteProgram(this->_program);
    }

    template<class gl_shader_class>
    void attach(gl_shader_class& shader)
    {
        glAttachShader(this->_program, shader.c_shader());
    }

    void link()
    {
        glLinkProgram(this->_program);
    }

    void use()
    {
        glUseProgram(this->_program);
    }

    
    GLint attrib_location(const std::string& attr_name)
    {
        return glGetAttribLocation(this->_program, attr_name.c_str());
    }
    GLint attrib_location(const char* attr_name)
    {
        return glGetAttribLocation(this->_program, attr_name);
    }
    GLint uniform_location(const std::string& uniform_name)
    {
        return glGetUniformLocation(this->_program, uniform_name.c_str());
    }
    GLint uniform_location(const char* uniform_name)
    {
        return glGetUniformLocation(this->_program, uniform_name);
    }
};





}

GLuint LoadShader(const char *shaderSrc, GLenum type)
{
    GLuint shader;
    GLint compiled;

    // Create the shader object
    shader = glCreateShader(type);
    if (shader == 0)
        return 0;
    // Load the shader source
    glShaderSource(shader, 1, &shaderSrc, NULL);

    // Compile the shader
    glCompileShader(shader);
    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 1)
        {
            char *infoLog = (char*)malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            std::printf("Error compiling shader:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}


std::string vtx_source = GLSL_STRINGIZE_SOURCE(100,

attribute vec4 position;
varying mediump vec2  pos;         
uniform vec4 offset;      
                            
void main()                        
{                                  
    gl_Position = position + offset;
    pos = position.xy;              
}                                  

);

std::string frag_source = GLSL_STRINGIZE_SOURCE(100,

varying mediump vec2 pos;
uniform mediump float phase;                   
                                            
void  main()                                     
{                                                
    gl_FragColor  =  vec4( 1., 0.9, 0.7, 1.0 ) *  
    cos( 30. * sqrt(pos.x*pos.x + 1.5 * pos.y * pos.y)
             + atan(pos.y,pos.x) - phase );      
}

);

int main(int argc, char **argv)
{
    gl::context ctx;
    gl::fragment_shader frag(frag_source);
    gl::vertex_shader vtx(vtx_source);    
    if (!frag.compile()) { std::cerr << "fucking frag" << std::endl; return 1; }
    if (!vtx.compile()) { std::cerr << "fucking vertex" << std::endl; return 1; }
    gl::program shader_program;
    shader_program.attach(vtx);
    shader_program.attach(frag);
    shader_program.link();
    shader_program.use();

    GLint position_loc = shader_program.attrib_location("position");
    GLint phase_loc = shader_program.uniform_location("phase");
    GLint offset_loc = shader_program.uniform_location("offset");
    
    if (position_loc < 0  ||  phase_loc < 0  ||  offset_loc < 0)
    {
        std::cerr << "Unable to get uniform location" << std::endl;
        return 1;
    }

    GLfloat vVertices[] = {  0.0f,  0.5f, 0.0f,
                            -0.5f, -0.5f, 0.0f,
                            0.5f, -0.5f, 0.0f
                         };

    while (true)
    {
        glClear ( GL_COLOR_BUFFER_BIT );
        glViewport (0, 0, 1920, 1080);
        glClearColor(0.5, 0.5, 0.5, 1.0);
        glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, vVertices );
        glEnableVertexAttribArray ( 0 );

        glDrawArrays ( GL_TRIANGLES, 0, 3 );
    }

    return 0;
}
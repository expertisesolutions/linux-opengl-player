/* (c) Copyright 2011-2014 Felipe Magno de Almeida
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define GL_GLEXT_PROTOTYPES

#ifdef GHTV_USE_GLUT
#include <GL/glut.h>
#endif
#ifdef GHTV_USE_OPENGLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef GHTV_BCM_HOST
#include <bcm_host.h>
#endif

#include <ghtv/opengl/linux/handle_events.hpp>
#include <ghtv/opengl/linux/idle_update.hpp>
#include <ghtv/opengl/linux/draw.hpp>
#include <ghtv/opengl/main_state.hpp>
#include <ghtv/opengl/linux/global_state.hpp>
#include <ghtv/opengl/linux/static_texture_player.hpp>
#include <ghtv/opengl/linux/binary_file.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <gntl/parser/libxml2/dom/xml_document.hpp>
#include <gntl/parser/libxml2/dom/document.hpp>

#include <gntl/algorithm/structure/context/start_normal_action.hpp>
#include <gntl/algorithm/structure/context/start.ipp>
#include <gntl/algorithm/structure/context/start_action_traits.ipp>
#include <gntl/algorithm/structure/context/stop_normal_action.hpp>
#include <gntl/algorithm/structure/context/stop.ipp>
#include <gntl/algorithm/structure/context/stop_action_traits.ipp>
#include <gntl/algorithm/structure/context/resume_normal_action.hpp>
#include <gntl/algorithm/structure/context/resume.ipp>
#include <gntl/algorithm/structure/context/resume_action_traits.ipp>
#include <gntl/algorithm/structure/context/pause_normal_action.hpp>
#include <gntl/algorithm/structure/context/pause.ipp>
#include <gntl/algorithm/structure/context/pause_action_traits.ipp>
#include <gntl/algorithm/structure/context/abort_normal_action.hpp>
#include <gntl/algorithm/structure/context/abort.ipp>
#include <gntl/algorithm/structure/context/abort_action_traits.ipp>

#include <gntl/algorithm/structure/port/start_action_traits.ipp>
#include <gntl/algorithm/structure/port/start.ipp>
#include <gntl/algorithm/structure/component/start.ipp>
#include <gntl/algorithm/structure/component/stop.ipp>
#include <gntl/algorithm/structure/component/pause.ipp>
#include <gntl/algorithm/structure/component/resume.ipp>
#include <gntl/algorithm/structure/component/abort.ipp>
#include <gntl/algorithm/structure/media/start_normal_action.hpp>
#include <gntl/algorithm/structure/media/start.ipp>
#include <gntl/algorithm/structure/media/start_action_traits.ipp>
#include <gntl/algorithm/structure/media/stop.ipp>
#include <gntl/algorithm/structure/media/stop_action_traits.ipp>
#include <gntl/algorithm/structure/media/resume.ipp>
#include <gntl/algorithm/structure/media/resume_action_traits.ipp>
#include <gntl/algorithm/structure/media/pause.ipp>
#include <gntl/algorithm/structure/media/pause_action_traits.ipp>
#include <gntl/algorithm/structure/media/abort.ipp>
#include <gntl/algorithm/structure/media/abort_action_traits.ipp>
#include <gntl/algorithm/structure/media/set.ipp>
#include <gntl/algorithm/structure/media/set_action_traits.ipp>
#include <gntl/algorithm/structure/media/select.ipp>
#include <gntl/algorithm/structure/switch/start_normal_action.hpp>
#include <gntl/algorithm/structure/switch/start.ipp>
#include <gntl/algorithm/structure/switch/start_action_traits.ipp>
#include <gntl/algorithm/structure/switch/stop.ipp>
#include <gntl/algorithm/structure/switch/stop_action_traits.ipp>
#include <gntl/algorithm/structure/switch/abort.ipp>
#include <gntl/algorithm/structure/switch/abort_action_traits.ipp>
#include <gntl/algorithm/structure/context/evaluate_links.ipp>

#include <iostream>
#include <cassert>
#include <fstream>

#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>

using ghtv::opengl::linux_::global_state;

namespace ghtv { namespace opengl { namespace linux_ {

state global_state;
      
#ifndef GHTV_USE_GLUT
bool no_draw = false;
#endif
      
void handle_key(std::string const& key, bool pressed = true)
{
  //std::cout << "handling key " << key << std::endl;
  typedef gntl::concept::structure::document_traits
    <state::main_state_type::document_type> structure_document_traits;
  typedef structure_document_traits::property_type property_type;
  typedef gntl::concept::structure::property_traits<property_type> property_traits;
  
  bool processed_current_key_master = false;
  if(structure_document_traits::has_property
     (*global_state.main_state.document, "service.currentFocus")
     && property_traits::is_integer
     (structure_document_traits::get_property
      (*global_state.main_state.document, "service.currentFocus"))
     && structure_document_traits::focused_media(*global_state.main_state.document)
     == structure_document_traits::current_key_master(*global_state.main_state.document)
     )
  {
    //std::cout << "Has currentFocus" << std::endl;
  }

  if(!processed_current_key_master)
  {
    //std::cout << "!processed_current_key_master" << std::endl;
    typedef structure_document_traits::media_lookupable
      media_lookupable;
    media_lookupable lookupable = structure_document_traits::media_lookup
      (*global_state.main_state.document);
    typedef gntl::concept::lookupable_traits<media_lookupable> lookupable_traits;
    typedef lookupable_traits::result_type lookup_result;
    lookup_result r = lookupable_traits::lookup
      (lookupable
       , structure_document_traits::current_key_master(*global_state.main_state.document));
    if(r != lookupable_traits::not_found(lookupable))
    {
      typedef lookupable_traits::value_type media_value_type;
      typedef gntl::unwrap_parameter<media_value_type>::type media_type;
      typedef gntl::concept::structure::media_traits<media_type> media_traits;
      typedef media_traits::presentation_range presentation_range;
      presentation_range presentations = media_traits::presentation_all(*r);
      typedef boost::range_iterator<presentation_range>::type presentation_iterator;
      for(presentation_iterator first = boost::begin(presentations)
            , last = boost::end(presentations); first != last; ++first)
      {
        typedef boost::range_value<presentation_range>::type presentation_type;
        typedef gntl::concept::structure::presentation_traits<presentation_type> presentation_traits;
        if(presentation_traits::is_occurring(*first))
        {
          std::cout << "processing current key master " << std::endl;
          boost::unwrap_ref(boost::unwrap_ref(*first).executor())
            .key_process(key, pressed);
          processed_current_key_master = true;
          break;
        }
        else
        {
          std::cout << "Not found a running presentation" << std::endl;
        }
      }
    }
  }
  else
    std::cout << "yes processed_current_key_master" << std::endl;

  if(true && !processed_current_key_master)
  {
    std::cout << "yet not processed_current_key_master" << std::endl;
    gntl::algorithm::structure::media::dimensions screen_dimensions
      = {0, 0, global_state.width, global_state.height, 0};
    gntl::algorithm::structure::context::evaluate_select_links
      (gntl::ref(global_state.main_state.document->body)
       , global_state.main_state.document->body_location()
       , gntl::ref(*global_state.main_state.document)
       , key, screen_dimensions);
    if(structure_document_traits::has_focus (*global_state.main_state.document)
       && structure_document_traits::is_focus_bound(*global_state.main_state.document))
    {
      typedef structure_document_traits::media_lookupable media_lookupable;
      media_lookupable lookupable = structure_document_traits::media_lookup(*global_state.main_state.document);
      typedef gntl::concept::lookupable_traits<media_lookupable> lookupable_traits;
      typedef lookupable_traits::result_type lookup_result;

      lookup_result r = lookupable_traits::lookup
        (lookupable
         , structure_document_traits::focused_media(*global_state.main_state.document));

      if(r != lookupable_traits::not_found(lookupable))
      {
        std::cout << "Focused media " << gntl::concept::identifier(*r) << std::endl;
        gntl::algorithm::structure::media::focus_select
          (*r, gntl::ref(*global_state.main_state.document), key);
      }
      else
        std::cout << "Couldn't find focused media" << std::endl;
    }      
  }
}
      
#ifdef GHTV_USE_GLUT
void charKeyEvent(unsigned char k, bool pressed)
{
  std::string key;
  switch(k)
  {
  case '1':
  case '2':
  case '3':
  case '4':
    key = k;
    break;
  case 8:
    // std::cout << "backspace" << std::endl;
    key = "BACK";
    break;
  case '\r':
    key = "ENTER";
    break;
  default:
    return;
  }
  handle_key(key, pressed);
  handle_events();
  draw();
}
void keyPressed(unsigned char k, int x, int y)
{
  charKeyEvent(k, true);
}
void keyReleased(unsigned char k, int x, int y)
{
  charKeyEvent(k, false);
}

void specialKeyEvent(int k, bool pressed)
{
  // std::cout << "special Key " << k << std::endl;
  std::string key;
  switch(k)
  {
  case GLUT_KEY_F1:
    key = "RED";
    break;
  case GLUT_KEY_F2:
    key = "GREEN";
    break;
  case GLUT_KEY_F3:
    key = "YELLOW";
    break;
  case GLUT_KEY_F4:
    key = "BLUE";
    break;
  case GLUT_KEY_LEFT:
    key = "CURSOR_LEFT";
    break;
  case GLUT_KEY_RIGHT:
    key = "CURSOR_RIGHT";
    break;
  case GLUT_KEY_UP:
    key = "CURSOR_UP";
    break;
  case GLUT_KEY_DOWN:
    key = "CURSOR_DOWN";
    break;
  case GLUT_KEY_F10:
    exit(0);
  default:
    return;
  }
  handle_key(key, pressed);
  handle_events();
  draw();
}
void specialKeyPressed(int k, int x, int y)
{
  specialKeyEvent(k, true);
}
void specialKeyReleased(int k, int x, int y)
{
  specialKeyEvent(k, false);
}
#endif

} } }

int main(int argc, char* argv[])
{
#ifdef GHTV_USE_GLUT
  glutInit(&argc, argv);
#endif

  boost::filesystem::path file_path;
  std::string input_path;
  {
    boost::program_options::options_description description("Allowed options");
    description.add_options()
      ("help", "Produce help message")
      ("ncl", boost::program_options::value<std::string>(), "NCL file")
#ifdef GHTV_RASPBERRYPI
      ("input", boost::program_options::value<std::string>(&input_path)->default_value("/dev/event1"), "Which /dev/input/* file to open for input")
#endif
      ;
    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, description), vm);
    boost::program_options::notify(vm);

    if(vm.count("help"))
    {
      std::cout << description << std::endl;
      return 1;
    }
    if(vm.count("ncl"))
      file_path = vm["ncl"].as<std::string>();
    else
    {
      std::cout << "You must specify the ncl parameter" << std::endl;
      return -1;
    }
  }

  using ghtv::opengl::linux_::state;

  {
    boost::filesystem::path root_path = file_path.parent_path();
    std::cout << "root_path: " << root_path << std::endl;
    ::xmlDocPtr xmldoc = xmlParseFile(file_path.string().c_str());
    global_state.xml_document.reset(new gntl::parser::libxml2::dom::xml_document(xmldoc));

    global_state.main_state.parser_document.reset
      (new gntl::parser::libxml2::dom::document (global_state.xml_document->root ()));

    state::main_state_type::presentation_factory_type 
      factory(root_path, global_state.main_state.active_players);
    std::map<std::string, state::main_state_type::document_type> imported_documents;
    global_state.main_state.document.reset
      (new state::main_state_type::document_type
       (*global_state.main_state.parser_document
        , "/", factory, imported_documents));

    ghtv::opengl::linux_::binary_file::initialize_binary_files();
  }  
#ifdef GHTV_USE_GLUT
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);

  glutInitWindowSize(global_state.width, global_state.height);

  glutInitWindowPosition(0, 0);

  int window = glutCreateWindow("GHTV Opengl Player");
  static_cast<void>(window);
#endif
#ifdef GHTV_BCM_HOST
  {
    bcm_host_init();
    std::atexit(bcm_host_deinit);
  
    global_state.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert( global_state.egl_display != EGL_NO_DISPLAY);

    EGLint major, minor;
    if(!eglInitialize(global_state.egl_display, &major, &minor))
    {
      std::cout << "Failed initializing display" << std::endl;
      return 1;
    }

    const EGLint attribs[] =
      {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
      };
    EGLint numConfigs;
    EGLConfig config;

    if(!eglChooseConfig(global_state.egl_display, attribs, &config, 1, &numConfigs))
    {
      std::cout << "Choosing config failed" << std::endl;
      return 1;
    }

    EGLBoolean result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);
    assert(glGetError() == 0);

    {
      EGLint attribute_list[] = 
        {
          EGL_CONTEXT_CLIENT_VERSION, 2
          , EGL_NONE
        };

      // create an EGL rendering context
      global_state.egl_context
        = eglCreateContext( global_state.egl_display, config, EGL_NO_CONTEXT, attribute_list);
      assert(global_state.egl_context!=EGL_NO_CONTEXT);

      uint32_t screen_width, screen_height;

      // create an EGL window surface
      int32_t success = graphics_get_display_size(0 /* LCD */, &screen_width, &screen_height);
      assert( success >= 0 );

      VC_RECT_T dst_rect;
      VC_RECT_T src_rect;

      dst_rect.x = 0;
      dst_rect.y = 0;
      dst_rect.width = screen_width;
      dst_rect.height = screen_height;
      
      src_rect.x = 0;
      src_rect.y = 0;
      src_rect.width = screen_width << 16;
      src_rect.height = screen_height << 16;        

      DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
      DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start( 0 );
         
      DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add
        ( dispman_update, dispman_display
          , 0/*layer*/, &dst_rect, 0/*src*/
          , &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, DISPMANX_TRANSFORM_T(0)/*transform*/);

      
      global_state.nativewindow.element = dispman_element;
      global_state.nativewindow.width = screen_width;
      global_state.nativewindow.height = screen_height;
      vc_dispmanx_update_submit_sync( dispman_update );

      assert(glGetError() == 0);

      global_state.width = screen_width;
      global_state.height = screen_height;
    }  

    global_state.egl_surface = eglCreateWindowSurface(global_state.egl_display, config
                                                      , & global_state.nativewindow, NULL);

    if (eglMakeCurrent(global_state.egl_display, global_state.egl_surface
                       , global_state.egl_surface, global_state.egl_context) == EGL_FALSE)
    {
      std::cout << "Failed making current surface" << std::endl;
      return 1;
    }

    glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
    glClear( GL_COLOR_BUFFER_BIT );
    assert(glGetError() == 0);

    global_state.reset_image_pipelines.reserve(1u);
    global_state.free_image_pipelines.resize(1u);
    for(std::vector<ghtv::opengl::linux_::state::image_pipeline>::iterator
          first = global_state.free_image_pipelines.begin()
          , last = global_state.free_image_pipelines.end()
          ; first != last; ++first)
      first->reset(new ghtv::omx_rpi::image_pipeline);
    
  }
#endif

  glEnable(GL_DEPTH_TEST);
  glDepthRange(0.f, 1.f);
  glDepthFunc(GL_LEQUAL);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLchar vertex_shader_src[]
    = "attribute vec2 vPosition;                            \n"
      "attribute vec2 a_texCoord;                           \n"
      "uniform float zindex;                                \n"
      "uniform mat4 projection_matrix;                      \n"
      "varying vec2 v_texCoord;                             \n"
      "void main()                                          \n"
      "{                                                    \n"
      "    vec4 position = vec4(vPosition, zindex, 1.0);    \n"
      "    gl_Position = projection_matrix*position;        \n"
      "    v_texCoord = a_texCoord;                         \n"
      "}                                                    \n"
    ;

  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);

  GLchar const* vertex_shader_src_tmp = vertex_shader_src;
  glShaderSource(vertex_shader, 1, &vertex_shader_src_tmp, 0);
  glCompileShader(vertex_shader);
  GLint compiled = 0;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
  assert(!!compiled);

  GLchar fragment_shader_src[]
    = "uniform vec4 color;                            \n"
      "uniform sampler2D s_texture;                   \n"
      "varying vec2 v_texCoord;                       \n"
      "uniform vec4 u_borderColor;                    \n"
      "uniform vec4 u_clip;                           \n"
      "void main()                                    \n"
      "{                                              \n"
      "    vec4 color;                                \n"
      "    if(v_texCoord.x >= u_clip.x                \n"
      "    && v_texCoord.y >= u_clip.y                \n"
      "    && v_texCoord.x <= u_clip.z                \n"
      "    && v_texCoord.y <= u_clip.w) {             \n"
      "      color = texture2D(s_texture, v_texCoord);\n"
      "    } else if(v_texCoord.x >= 0.0              \n"
      "    && v_texCoord.y >= 0.0                     \n"
      "    && v_texCoord.x <= 1.0                     \n"
      "    && v_texCoord.y <= 1.0) {                  \n"
      "      color = vec4(0.0, 0.0, 0.0, 0.0);        \n"
      "    } else {                                   \n"
      "      color = u_borderColor;                   \n"
      "    }                                          \n"
      "    gl_FragColor = color;                      \n"
      "}                                              \n"
    ;

  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  GLchar const* fragment_shader_src_tmp = fragment_shader_src;
  glShaderSource(fragment_shader, 1, &fragment_shader_src_tmp, 0);
  glCompileShader(fragment_shader);
  compiled = 0;
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compiled);
  assert(!!compiled);

  global_state.program_object = glCreateProgram();
  assert(!!global_state.program_object);

  glAttachShader(global_state.program_object, vertex_shader);
  glAttachShader(global_state.program_object, fragment_shader);

  glBindAttribLocation(global_state.program_object, 0, "vPosition");
  glBindAttribLocation(global_state.program_object, 1, "a_texCoord");

  glLinkProgram(global_state.program_object);

  GLint linked;
  glGetProgramiv(global_state.program_object, GL_LINK_STATUS, &linked);
  assert(!!linked);

#ifdef GHTV_USE_GLUT
  glutDisplayFunc(&ghtv::opengl::linux_::draw);

  glutKeyboardFunc(&ghtv::opengl::linux_::keyPressed);
  glutKeyboardUpFunc(&ghtv::opengl::linux_::keyReleased);
  glutSpecialFunc(&ghtv::opengl::linux_::specialKeyPressed);
  glutSpecialUpFunc(&ghtv::opengl::linux_::specialKeyReleased);
#endif

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  global_state.projection_location = glGetUniformLocation(global_state.program_object
                                                          , "projection_matrix");
  assert(global_state.projection_location != -1);

  global_state.z_location = glGetUniformLocation(global_state.program_object, "zindex");
  assert(global_state.z_location != -1);

  global_state.texture_location = glGetUniformLocation(global_state.program_object, "s_texture");
  assert(global_state.texture_location != -1);

  global_state.border_color_location = glGetUniformLocation(global_state.program_object, "u_borderColor");
  assert(global_state.border_color_location != -1);
  
  global_state.clip_location = glGetUniformLocation(global_state.program_object, "u_clip");
  assert(global_state.clip_location != -1);

  glUseProgram(global_state.program_object);

  assert(glGetAttribLocation(global_state.program_object, "vPosition") == 0);
  assert(glGetAttribLocation(global_state.program_object, "a_texCoord") == 1);

  float right = global_state.width
    , bottom = global_state.height
    ;

  float projection_matrix[16] =
    {  2.0f/right, 0.0f        , 0.0f, 0.0f
     , 0.0f      , -2.0f/bottom, 0.0f, 0.0f
     , 0.0f      , 0.0f        , -1.0f, 0.0f
     , -1.0f     , 1.0f        , 0.0f, 1.0f };
  glUniformMatrix4fv(global_state.projection_location, 1, false, projection_matrix);

  glViewport(0, 0, global_state.width, global_state.height);

  gntl::algorithm::structure::media::dimensions const screen_dimensions
    = {0, 0, global_state.width, global_state.height, 0};
  state::main_state_type::descriptor_type descriptor;
  gntl::algorithm::structure::context::start
    (gntl::ref(global_state.main_state.document->body)
     , global_state.main_state.document->body_location()
     , descriptor
     , gntl::ref(*global_state.main_state.document)
     , screen_dimensions);

#ifndef GHTV_USE_GLUT
  int input = ::open(input_path.c_str(), O_RDONLY);
  if(input < 0)
  {
    std::cerr << "Couldn't open input " << input_path << std::endl;
    return -1;
  }

  char name[256] = "Unknown";

  ioctl (input, EVIOCGNAME (sizeof (name)), name);
  
  bool quit = false;
  while (!quit)
  {
    ghtv::opengl::linux_::handle_events();
#endif
    ghtv::opengl::linux_::draw();

#ifndef GHTV_USE_GLUT
    struct input_event ev;
    
    // std::cout << "Reading From : " << input_path << " (" << name << ")" << std::endl;
    // std::cout << "reading key" << std::endl;
    if (read (input, &ev, sizeof(ev)) < sizeof(ev))
    {
      std::cout << "Error reading input" << std::endl;
      return -1;
    }
    using ghtv::opengl::linux_::handle_key;
      
    if (ev.value == 0 && ev.type == EV_KEY)
    {
      // std::cout << "read key " << ev.code << std::endl;

      switch(ev.code)
      {
      case KEY_F1:
        handle_key("RED");
        break;
      case KEY_F2:
        handle_key("GREEN");
        break;
      case KEY_F3:
        handle_key("YELLOW");
        break;
      case KEY_F4:
        handle_key("BLUE");
        break;
      case KEY_LEFT:
        handle_key("CURSOR_LEFT");
        break;
      case KEY_RIGHT:
        handle_key("CURSOR_RIGHT");
        break;
      case KEY_UP:
        handle_key("CURSOR_UP");
        break;
      case KEY_DOWN:
        handle_key("CURSOR_DOWN");
        break;
      case KEY_ENTER:
        handle_key("ENTER");
        break;
      // case KEY_F10:
      //   quit = true;
      //   break;
      case KEY_F9:
        ghtv::opengl::linux_::no_draw = !ghtv::opengl::linux_::no_draw;
        break;
      }
    }
  }
#else
  glutIdleFunc(ghtv::opengl::linux_::glut_idle_update_function);
  glutMainLoop();
#endif
  
  global_state.main_state.active_players.clear();
  global_state.main_state.document.reset();
  global_state.main_state.parser_document.reset();
  global_state.xml_document.reset();
  return 0;
}

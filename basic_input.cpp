#include "framework.h"

#include <set>

using namespace prototyper;

namespace input
{
#define MOUSE_BUTTON_ADD_VAL 0x10000

  enum action
  {
    ACTION_ONE,
    ACTION_TWO,
    ACTION_THREE,
    ACTION_FOUR,
    ACTION_FIVE,
    ACTION_SIX,
    ACTION_SEVEN,
    ACTION_EIGHT,
    ACTION_NINE,
    ACTION_TEN
  };

  enum state
  {
    STATE_ONE
  };

  enum range
  {
    RANGE_ONE,
    RANGE_TWO
  };

  class range_converter
  {
    private:
      struct converter
      {
        vec2 in;
        vec2 out;

        converter( const vec2& i = vec2(), const vec2& o = vec2() ) : in( i ), out( o ) {}

        template< class t >
        t convert( t inval ) const
        {
          float cval = clamp( inval, in.x, in.y );
          float factor = ( cval - in.x ) / ( in.y - in.x );
          return mix( out.x, out.y, factor );
        }
      };

      std::map< range, converter > conversionmap;

    public:
      void add_converter( range r, const vec2& in, const vec2& out )
      {
        if( in.y < in.x || out.y < out.x )
          return;

        conversionmap[r] = converter( in, out );
      }

      template< class t >
      t convert( range r, t inval ) const
      {
        try
        {
          converter it = conversionmap.at( r );
          return it.convert( inval );
        }
        catch( ... )
        {
          return inval;
        }
      }
  };

  class context
  {
    public:
      std::map< unsigned, action > actionmap;
      std::map< unsigned, state > statemap;
      std::map< unsigned, range > rangemap;

      std::map< range, float > sensitivitymap;
      range_converter conversion;

    protected:

    public:
      void add( unsigned code, action a )
      {
        actionmap[code] = a;
      }

      void add( unsigned code, state a )
      {
        statemap[code] = a;
      }

      void add( unsigned code, range a )
      {
        rangemap[code] = a;
      }

      void add_sensitivity( range code, float a )
      {
        sensitivitymap[code] = a;
      }

      void set_converter( const range_converter& r )
      {
        conversion = r;
      }

      bool map( unsigned code, action& out ) const
      {
        try
        {
          out = actionmap.at( code );
          return true;
        }
        catch( ... )
        {
          return false;
        }
      }

      bool map( unsigned code, state& out ) const
      {
        try
        {
          out = statemap.at( code );
          return true;
        }
        catch( ... )
        {
          return false;
        }
      }

      bool map( unsigned code, range& out ) const
      {
        try
        {
          out = rangemap.at( code );
          return true;
        }
        catch( ... )
        {
          return false;
        }
      }

      float get_sensitivity( const range& r ) const
      {
        try
        {
          float val = sensitivitymap.at( r );
          return val;
        }
        catch( ... )
        {
          return 1.0f;
        }
      }

      const range_converter& get_conversions() const
      {
        return conversion;
      }
  };

  struct mappedinput
  {
    std::set< action > actions;
    std::set< state > states;
    std::map< range, float > ranges;

    void consume( action a )
    {
      actions.erase( a );
    }

    void consume( state s )
    {
      states.erase( s );
    }

    void consume( range r )
    {
      auto it = ranges.find( r );

      if( it != ranges.end() )
        ranges.erase( it );
    }
  };

  class callback_base
  {
    public:
      virtual void operator()( mappedinput& ) {}
      virtual callback_base* clone()
      {
        return 0;
      }
  };

  template< class t >
  class callback : public callback_base
  {
      t func;
    public:
      void operator()( mappedinput& d )
      {
        func( d );
      }
      callback* clone()
      {
        return new callback( *this );
      }
      callback( t f ) : func( f ) {}
  };

  class mapper
  {
    private:
      std::map< std::wstring, context > contexts;
      std::list< context* > active_contexts;

      //priority, callback
      std::multimap< unsigned, callback_base* > callbacks; //allows multiple callback for one trigger

      mappedinput current_mappedinput;

      bool map( unsigned code, action& out ) const
      {
        for( auto it = active_contexts.begin(); it != active_contexts.end(); ++it )
        {
          if( ( *it )->map( code, out ) )
            return true;
        }

        return false;
      }

      bool map( unsigned code, state& out ) const
      {
        for( auto it = active_contexts.begin(); it != active_contexts.end(); ++it )
        {
          if( ( *it )->map( code, out ) )
            return true;
        }

        return false;
      }

      void map_and_consume( unsigned code )
      {
        action a;
        state s;

        if( map( code, a ) )
          current_mappedinput.consume( a );

        if( map( code, s ) )
          current_mappedinput.consume( s );
      }

    public:
      void add_context( const std::wstring& name, const context& c )
      {
        contexts[name] = c;
      }

      void add_button_event( unsigned button, bool pressed, bool prev_down )
      {
        action a;
        state s;

        if( pressed && !prev_down )
        {
          if( map( button, a ) )
          {
            current_mappedinput.actions.insert( a );
            return;
          }
        }

        if( pressed )
        {
          if( map( button, s ) )
          {
            current_mappedinput.states.insert( s );
            return;
          }
        }

        map_and_consume( button );
      }

      void add_axis_event( unsigned axis, float val )
      {
        for( auto it = active_contexts.begin(); it != active_contexts.end(); ++it )
        {
          range r;

          if( ( *it )->map( axis, r ) )
          {
            current_mappedinput.ranges[r] = ( *it )->get_conversions().convert( r, val * ( *it )->get_sensitivity( r ) );
            break;
          }
        }
      }

      template< class t >
      void add_callback( unsigned priority, t cb )
      {
        callbacks.insert( make_pair( priority, new callback< t >( cb ) ) );
      }

      void dispatch_callbacks() const
      {
        mappedinput input = current_mappedinput;

        for( auto it = callbacks.begin(); it != callbacks.end(); ++it )
          ( *it->second )( input );
      }

      void push_context( const std::wstring& name )
      {
        try
        {
          auto c = &contexts.at( name );
          active_contexts.push_front( c );
        }
        catch( ... ) {}
      }

      void pop_context()
      {
        if( !active_contexts.empty() )
          active_contexts.pop_front();
      }

      void clear()
      {
        current_mappedinput.actions.clear();
        current_mappedinput.ranges.clear();
      }

      ~mapper()
      {
        for( auto c = callbacks.begin(); c != callbacks.end(); ++c )
        {
          delete c->second;
        }
      }
  };
}

int main( int argc, char** argv )
{
  map<string, string> args;

  for( int c = 1; c < argc; ++c )
  {
    args[argv[c]] = c + 1 < argc ? argv[c + 1] : "";
    ++c;
  }

  cout << "Arguments: " << endl;
  for_each( args.begin(), args.end(), []( pair<string, string> p )
  {
    cout << p.first << " " << p.second << endl;
  } );

  uvec2 screen( 0 );
  bool fullscreen = false;
  bool silent = false;
  string title = "Input mapping";

  /*
   * Process program arguments
   */

  stringstream ss;
  ss.str( args["--screenx"] );
  ss >> screen.x;
  ss.clear();
  ss.str( args["--screeny"] );
  ss >> screen.y;
  ss.clear();

  if( screen.x == 0 )
  {
    screen.x = 1280;
  }

  if( screen.y == 0 )
  {
    screen.y = 720;
  }

  try
  {
    args.at( "--fullscreen" );
    fullscreen = true;
  }
  catch( ... ) {}

  try
  {
    args.at( "--help" );
    cout << title << ", written by Marton Tamas." << endl <<
         "Usage: --silent      //don't display FPS info in the terminal" << endl <<
         "       --screenx num //set screen width (default:1280)" << endl <<
         "       --screeny num //set screen height (default:720)" << endl <<
         "       --fullscreen  //set fullscreen, windowed by default" << endl <<
         "       --help        //display this information" << endl;
    return 0;
  }
  catch( ... ) {}

  try
  {
    args.at( "--silent" );
    silent = true;
  }
  catch( ... ) {}

  /*
   * Initialize the OpenGL context
   */

  framework frm;
  frm.init( screen, title, fullscreen );

  //set opengl settings
  glEnable( GL_DEPTH_TEST );
  glDepthFunc( GL_LEQUAL );
  glFrontFace( GL_CCW );
  glEnable( GL_CULL_FACE );
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
  glClearDepth( 1.0f );

  frm.get_opengl_error();

  /*
   * Set up mymath
   */

  camera<float> cam;
  frame<float> the_frame;

  float cam_fov = 45.0f;
  float cam_near = 1.0f;
  float cam_far = 100.0f;

  the_frame.set_perspective( radians( cam_fov ), ( float )screen.x / ( float )screen.y, cam_near, cam_far );

  glViewport( 0, 0, screen.x, screen.y );

  /*
   * Set up the scene
   */

  float move_amount = 5;
  float cam_rotation_amount = 5.0;

  GLuint box = frm.create_box();

  /*
   * Set up the shaders
   */

  GLuint debug_shader = 0;
  frm.load_shader( debug_shader, GL_VERTEX_SHADER, "../shaders/debug/debug.vs" );
  frm.load_shader( debug_shader, GL_FRAGMENT_SHADER, "../shaders/debug/debug.ps" );

  GLint debug_mvp_mat_loc = glGetUniformLocation( debug_shader, "mvp" );

  /*
   * Set up input handling
   */

  input::range_converter r1;
  r1.add_converter( input::RANGE_ONE, vec2( 0, screen.x ), vec2( 0, 1 ) );
  r1.add_converter( input::RANGE_TWO, vec2( 0, screen.y ), vec2( 0, 1 ) );

  input::context c1;
  c1.set_converter( r1 );
  c1.add( 0, input::RANGE_ONE ); //X axis
  c1.add( 1, input::RANGE_TWO ); //Y axis
  c1.add_sensitivity( input::RANGE_ONE, 1.0f );
  c1.add_sensitivity( input::RANGE_TWO, 1.0f );
  c1.add( sf::Keyboard::W, input::ACTION_ONE ); //move up
  c1.add( sf::Keyboard::A, input::ACTION_TWO ); //move left
  c1.add( sf::Keyboard::S, input::ACTION_THREE ); //move down
  c1.add( sf::Keyboard::D, input::ACTION_FOUR ); //move right
  c1.add( sf::Keyboard::Space, input::ACTION_NINE );

  //you can add different actions, ranges, and states to the same keyboard presses
  //this way when a context switch happens (user exits the menu)
  //suddenly he can control the car he sits in.
  //of course we need an action for context switching (action nine/ten)
  input::context c2;
  c2.set_converter( r1 );
  c2.add( 0, input::RANGE_ONE ); //X axis
  c2.add( 1, input::RANGE_TWO ); //Y axis
  c2.add_sensitivity( input::RANGE_ONE, 1.0f );
  c2.add_sensitivity( input::RANGE_TWO, 1.0f );
  c2.add( sf::Keyboard::W, input::ACTION_FIVE ); //move up
  c2.add( sf::Keyboard::A, input::ACTION_SIX ); //move left
  c2.add( sf::Keyboard::S, input::ACTION_SEVEN ); //move down
  c2.add( sf::Keyboard::D, input::ACTION_EIGHT ); //move right
  c2.add( sf::Keyboard::Space, input::ACTION_TEN );

  input::mapper m1;
  m1.add_context( L"main_context", c1 );
  m1.add_context( L"main_context2", c2 );
  m1.push_context( L"main_context" );
  m1.add_callback( 0,
                   [&]( input::mappedinput & d )
  {
    for( auto c = d.actions.begin(); c != d.actions.end(); ++c )
    {
      if( *c == input::ACTION_ONE ) //W
      {
        cam.move_forward( move_amount );
        d.consume( *c );
      }
      else if( *c == input::ACTION_TWO ) //A
      {
        cam.rotate_y( radians( cam_rotation_amount ) );
      }
      else if( *c == input::ACTION_THREE ) //S
      {
        cam.move_forward( -move_amount );
      }
      else if( *c == input::ACTION_FOUR ) //D
      {
        cam.rotate_y( radians( -cam_rotation_amount ) );
      }
      else if( *c == input::ACTION_NINE )
      {
        m1.pop_context();
        m1.push_context( L"main_context2" );
      }

      if( d.actions.empty() )
        break;
    }
  } );

  m1.add_callback( 1,
                   [&]( input::mappedinput & d )
  {
    for( auto c = d.actions.begin(); c != d.actions.end(); ++c )
    {
      if( *c == input::ACTION_ONE ) //W
      {
        cout << "callback1" << endl; //this never runs, due to callback0 consuming the action
        d.consume( *c );
      }

      if( d.actions.empty() )
        break;
    }
  } );

  m1.add_callback( 2,
                   [&]( input::mappedinput & d )
  {
    for( auto c = d.actions.begin(); c != d.actions.end(); ++c )
    {
      if( *c == input::ACTION_FIVE ) //W
      {
        cout << "w" << endl;
      }
      else if( *c == input::ACTION_SIX ) //A
      {
        cout << "a" << endl;
      }
      else if( *c == input::ACTION_SEVEN ) //S
      {
        cout << "s" << endl;
      }
      else if( *c == input::ACTION_EIGHT ) //D
      {
        cout << "d" << endl;
      }
      else if( *c == input::ACTION_TEN )
      {
        m1.pop_context();
        m1.push_context( L"main_context" );
      }
    }
  } );

  /*
   * Handle events
   */

  auto event_handler = [&]( const sf::Event & ev )
  {
    switch( ev.type )
    {
      case sf::Event::KeyPressed:
        {
          m1.add_button_event( ev.key.code, true, false );
          break;
        }
      case sf::Event::KeyReleased:
        {
          m1.add_button_event( ev.key.code, false, false );
          break;
        }
      case sf::Event::MouseButtonPressed:
        {
          m1.add_button_event( ev.mouseButton.button + MOUSE_BUTTON_ADD_VAL, true, false );
          break;
        }
      case sf::Event::MouseButtonReleased:
        {
          m1.add_button_event( ev.mouseButton.button + MOUSE_BUTTON_ADD_VAL, false, false );
          break;
        }
      case sf::Event::MouseMoved:
        {
          m1.add_axis_event( 0, ev.mouseMove.x );
          m1.add_axis_event( 1, ev.mouseMove.y );
        }
      default:
        break;
    }
  };

  /*
   * Render
   */

  sf::Clock timer;
  timer.restart();

  frm.display( [&]
  {
    frm.handle_events( event_handler );
    m1.dispatch_callbacks();
    m1.clear();

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glUseProgram( debug_shader );

    mat4 trans = create_translation( vec3( 0, 0, -5 ) );
    mat4 view = cam.get_matrix();
    mat4 projection = the_frame.projection_matrix;
    mat4 model = mat4::identity;
    mat4 mvp = projection * view * model * trans;
    glUniformMatrix4fv( debug_mvp_mat_loc, 1, false, &mvp[0][0] );

    glBindVertexArray( box );
    glDrawElements( GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0 );

    frm.get_opengl_error();
  }, silent );

  return 0;
}

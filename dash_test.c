/*  CHIP MAKER
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <png.h>

#include "bcm_host.h"
#include "vgfont.h"
#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#define PATH "./"
#define IMAGE_SIZE 100
#ifndef M_PI
   #define M_PI 3.141592654
#endif

float maxrpm = 7000, maxh = 35.0, minh = -25.0; // Test values for size of continuous bar
int valint;

static GLfloat quadx[4 * 4 * 3] = { //testing using full sized coordinates on (1920x1080)
   -96.f, -54.f,  0.f,
   96.f, -54.f,  0.f,
   -96.f,  54.f,  0.f, //background
   96.f,  54.f,  0.f,
   
   -14.f, -26.f,  0.f, //discrete lights
   14.f, -26.f,  0.f,
   -14.f,  26.f,  0.f,
   14.f,  26.f,  0.f,
   
   50.f, -27.f,  0.f, // top yvals: 31,34
   65.f, -27.f,  0.f, //continuous bar
   50.f,  27.f,  0.f,
   65.f,  27.f,  0.f,

   -57.5f, -23.f,  0.f, 
   -47.5f, -23.f,  0.f, //gauge
   -57.5f,  4.5f,  0.f,
   -47.5f,  4.5f,  0.f,
   
};

static const GLfloat texCoords[4 * 4 * 2] = {
   0.f,  0.f,
   1.f,  0.f,
   0.f,  1.f,
   1.f,  1.f,
   
   0.f,  0.f,
   1.f,  0.f,
   0.f,  1.f,
   1.f,  1.f,
   
   0.f,  0.f,
   1.f,  0.f,
   0.f,  1.f,
   1.f,  1.f,
   
   0.f,  0.f,
   1.f,  0.f,
   0.f,  1.f,
   1.f,  1.f,
   
};

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
   int light;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
   GLuint tex[12]; 
   GLfloat rot_angle_x_inc;
   GLfloat rot_angle_y_inc;
   GLfloat rot_angle_z_inc;
   GLfloat rot_angle_x;
   GLfloat rot_angle_y;
   GLfloat rot_angle_z;
   GLfloat distance;
   GLfloat distance_inc;
   char *tex_buf1;
   char *tex_buf2;
   char *tex_buf3;
} CUBE_STATE_T;

static void init_ogl(CUBE_STATE_T *state);
static void init_model_proj(CUBE_STATE_T *state);
static void reset_model(CUBE_STATE_T *state);
static void redraw_scene(CUBE_STATE_T *state);
static void update_model(CUBE_STATE_T *state);
static void init_textures(CUBE_STATE_T *state);
static void load_tex_images(CUBE_STATE_T *state);
static void exit_func(void);
static volatile int terminate;
static CUBE_STATE_T _state, *state=&_state;
int pngLoad(char *file, unsigned long **pwidth, unsigned long **pheight, char **image_data_ptr);
static const char *strnchr(const char *str, size_t len, char c);
int32_t render_subtitle(GRAPHICS_RESOURCE_HANDLE img, const char *text, const int skip, const uint32_t text_size, const uint32_t y_offset);


static void init_ogl(CUBE_STATE_T *state)
{
    //BROADCOM SPECIFIC
   int32_t success = 0;
   EGLBoolean result;
   EGLint num_config;

   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_UPDATE_HANDLE_T dispman_update;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   
   EGLConfig config;

   // get an EGL display connection
   state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(state->display!=EGL_NO_DISPLAY);

   // initialize the EGL display connection
   result = eglInitialize(state->display, NULL, NULL);
   assert(EGL_FALSE != result);

   // get an appropriate EGL frame buffer configuration
   result = eglChooseConfig(state->display, attribute_list, &config, 1, &num_config);
   assert(EGL_FALSE != result);

   // create an EGL rendering context
   state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, NULL);
   assert(state->context!=EGL_NO_CONTEXT);

   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
   assert( success >= 0 );
	
   printf("Width:%d Height:%d \n", state->screen_width, state->screen_height);	
	
   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = state->screen_width;
   dst_rect.height = state->screen_height;
      
   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = state->screen_width << 16;
   src_rect.height = state->screen_height << 16;        

   state->dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
   dispman_update = vc_dispmanx_update_start( 0 );
         
   state->dispman_element = vc_dispmanx_element_add ( dispman_update, state->dispman_display,
      0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, 0/*transform*/);
      
   nativewindow.element = state->dispman_element;
   nativewindow.width = state->screen_width;
   nativewindow.height = state->screen_height;
   vc_dispmanx_update_submit_sync( dispman_update );
      
   state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
   assert(state->surface != EGL_NO_SURFACE);

   // connect the context to the surface
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);

   // Set background color and clear buffers
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

   // Enable back face culling.
   glEnable(GL_CULL_FACE);

   glMatrixMode(GL_MODELVIEW);
}

static void init_model_proj(CUBE_STATE_T *state)
{
   float nearp = 1.0f;
   float farp = 500.0f;
   float hht;
   float hwd;

   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

   glViewport(0, 0, (GLsizei)state->screen_width, (GLsizei)state->screen_height);
      
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   hht = nearp * (float)tan(45.0 / 2.0 / 180.0 * M_PI);
   hwd = hht * (float)state->screen_width / (float)state->screen_height;

   glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);
   
   glEnableClientState( GL_VERTEX_ARRAY );
   glVertexPointer( 3, GL_FLOAT, 0, quadx );

   reset_model(state);
}

static void reset_model(CUBE_STATE_T *state)
{
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.f, 0.f, -50.f);
   
   state->distance = 130.f; // using a large coord system to accurately place (odd sizes doubled for even)
}

static void update_model(CUBE_STATE_T *state)
{
   
    state->light = floor(valint/1000);
    float valf = (float) valint;
    float yheight = (valf/maxrpm) * (maxh - minh) + minh;
    quadx[31] = yheight; quadx[34] = yheight;
    
   glLoadIdentity();
   glTranslatef(0.f, 0.f, -state->distance);

}


static void redraw_scene(CUBE_STATE_T *state)
{
   glClear( GL_COLOR_BUFFER_BIT );
    
   glBindTexture(GL_TEXTURE_2D, state->tex[0]);
   glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
   

		int i = state->light, j;
		if (i == 1 || i == 2 || i == 3) {j=11;}
		else if (i == 4 || i == 5) {j=10;}
		else if (i == 6 || i == 7) {j = 9;} else {}
		
		glBindTexture(GL_TEXTURE_2D, state->tex[(state->light)+1]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  if (i > 0) {
	  //glBindTexture(GL_TEXTURE_2D, state->tex[j]);
	//glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
	}
	
	glBindTexture(GL_TEXTURE_2D, state->tex[12]);
	float angle = ((float)valint / 10000) * -270;
	glTranslatef(-52.5f, 0.f, 0.f);
	glRotatef(angle, 0.0f,0.f, 1.0f);
	glTranslatef(52.5f, 0.f, 0.f);

	glDrawArrays( GL_TRIANGLE_STRIP, 12, 4);
/*if (state->light == 0) {
	glBindTexture(GL_TEXTURE_2D, state->tex[1]);
	glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	
   } else if (state->light == 1) {
	  glBindTexture(GL_TEXTURE_2D, state->tex[(state->light)+1]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  glBindTexture(GL_TEXTURE_2D, state->tex[11]);
	glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
   } else if (state->light == 2) {
	  glBindTexture(GL_TEXTURE_2D, state->tex[3]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  glBindTexture(GL_TEXTURE_2D, state->tex[11]);
	glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
   } else if (state->light == 3) {
	  glBindTexture(GL_TEXTURE_2D, state->tex[4]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  glBindTexture(GL_TEXTURE_2D, state->tex[11]);
	glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
   } else if (state->light == 4) {
	  glBindTexture(GL_TEXTURE_2D, state->tex[5]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  glBindTexture(GL_TEXTURE_2D, state->tex[10]);
	glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
   } else if (state->light == 5) {
	  glBindTexture(GL_TEXTURE_2D, state->tex[6]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  glBindTexture(GL_TEXTURE_2D, state->tex[10]);
	glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
   } else if (state->light == 6) {
	  glBindTexture(GL_TEXTURE_2D, state->tex[7]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  glBindTexture(GL_TEXTURE_2D, state->tex[9]);
	glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
   } else if (state->light == 7) {
	  glBindTexture(GL_TEXTURE_2D, state->tex[8]);
	  glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);
	  glBindTexture(GL_TEXTURE_2D, state->tex[9]);
	glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);
   } else {}*/

   eglSwapBuffers(state->display, state->surface);
}

static void init_textures(CUBE_STATE_T *state)
{	
	printf("Initialising Textures...\n");
   // load three texture buffers but use them on six OGL|ES texture surfaces
   int result;
   unsigned long *width, *height;
   //int image_sz = 0;
	 char *data; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result = pngLoad("bk2.png", &width, &height, &data);
   if (result == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
  
    int result2;
   unsigned long *width2, *height2;
   //int image_sz = 0;
	 char *data2; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result2 = pngLoad("light0.png", &width2, &height2, &data2);
   if (result2 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    
    int result3;
   unsigned long *width3, *height3;
   //int image_sz = 0;
	 char *data3; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result3 = pngLoad("light1.png", &width3, &height3, &data3);
   if (result3 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    
    int result4;
   unsigned long *width4, *height4;
   //int image_sz = 0;
	 char *data4; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result4 = pngLoad("light2.png", &width4, &height4, &data4);
   if (result4 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int result5;
   unsigned long *width5, *height5;
   //int image_sz = 0;
	 char *data5; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result5 = pngLoad("light3.png", &width5, &height5, &data5);
   if (result5 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int result6;
   unsigned long *width6, *height6;
   //int image_sz = 0;
	 char *data6; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result6 = pngLoad("light4.png", &width6, &height6, &data6);
   if (result6 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int result7;
   unsigned long *width7, *height7;
   //int image_sz = 0;
	 char *data7; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result7 = pngLoad("light5.png", &width7, &height7, &data7);
   if (result7 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int result8;
   unsigned long *width8, *height8;
   //int image_sz = 0;
	 char *data8; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result8 = pngLoad("light6.png", &width8, &height8, &data8);
   if (result8 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int result9;
   unsigned long *width9, *height9;
   //int image_sz = 0;
	 char *data9; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   result9 = pngLoad("light7.png", &width9, &height9, &data9);
   if (result9 == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int resultred;
   unsigned long *widthred, *heightred;
   //int image_sz = 0;
	 char *datared; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   resultred = pngLoad("red.png", &widthred, &heightred, &datared);
   if (resultred == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int resultyel;
   unsigned long *widthyel, *heightyel;
   //int image_sz = 0;
	 char *datayel; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   resultyel = pngLoad("yellow.png", &widthyel, &heightyel, &datayel);
   if (resultyel == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int resultgre;
   unsigned long *widthgre, *heightgre;
   //int image_sz = 0;
	 char *datagre; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   resultgre = pngLoad("green.png", &widthgre, &heightgre, &datagre);
   if (resultgre == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
    int resultgauge;
   unsigned long *widthgauge, *heightgauge;
   //int image_sz = 0;
	 char *datagauge; // DONT FORGET TO RELEASE- READ ABOUT INITIALISATION SIZE
   //state->tex_buf1 = malloc(image_sz);
   resultgauge = pngLoad("gauge.png", &widthgauge, &heightgauge, &datagauge);
   if (resultgauge == 0)
    {
 
      printf("(pngLoad) FAILED.\n");
    }
    
   glGenTextures(13, &state->tex[0]);
   // setup first texture
   glBindTexture(GL_TEXTURE_2D, state->tex[0]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1920, 1080, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);

   // setup 2nd texture l0
   glBindTexture(GL_TEXTURE_2D, state->tex[1]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data2);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);

   //l1
   glBindTexture(GL_TEXTURE_2D, state->tex[2]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data3);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //l2
   glBindTexture(GL_TEXTURE_2D, state->tex[3]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data4);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   

   //l3
   glBindTexture(GL_TEXTURE_2D, state->tex[4]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data5);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //l4
   glBindTexture(GL_TEXTURE_2D, state->tex[5]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data6);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //l5
   glBindTexture(GL_TEXTURE_2D, state->tex[6]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data7);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //l6
   glBindTexture(GL_TEXTURE_2D, state->tex[7]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data8);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //l7
   glBindTexture(GL_TEXTURE_2D, state->tex[8]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 280, 520, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data9);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //red
   glBindTexture(GL_TEXTURE_2D, state->tex[9]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 148, 550, 0,
                GL_RGB, GL_UNSIGNED_BYTE, datared);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //yellow
   glBindTexture(GL_TEXTURE_2D, state->tex[10]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 148, 550, 0,
                GL_RGB, GL_UNSIGNED_BYTE, datayel);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //green
   glBindTexture(GL_TEXTURE_2D, state->tex[11]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 148, 550, 0,
                GL_RGB, GL_UNSIGNED_BYTE, datagre);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
   
   //gauge
   glBindTexture(GL_TEXTURE_2D, state->tex[12]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 100, 275, 0,
                GL_RGB, GL_UNSIGNED_BYTE, datagauge);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);
   
   // setup overall texture environment
   glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   
   glEnable(GL_TEXTURE_2D);
   printf("Textures Initialised\n");
}

static void load_tex_images(CUBE_STATE_T *state)
{  /* LOADING PNGS DIRECTLY NOW */
    
	/*printf("	Loading Images..\n");
   FILE *tex_file1 = NULL;
   int bytes_read, image_sz = IMAGE_SIZE*IMAGE_SIZE*3;

   state->tex_buf1 = malloc(image_sz);

   tex_file1 = fopen(PATH "Lucca_128_128.raw", "rb");
   if (tex_file1 && state->tex_buf1)
   {
      bytes_read=fread(state->tex_buf1, 1, image_sz, tex_file1);
      assert(bytes_read == image_sz);  // some problem with file?
      fclose(tex_file1);
   }
   int result;
   unsigned long *width, *height;
   //int image_sz = 0;

   //state->tex_buf1 = malloc(image_sz);
   result = pngLoad("100x100.png", &width, &height, &state->tex_buf1);
   if (result == 0)
    {
 
      printf("(pngLoad) %s FAILED.\n", filename);
    }*/
   
}

int pngLoad(char *file, unsigned long **pwidth, 
	unsigned long **pheight, char **image_data_ptr)
{

   FILE         *infile;         /* PNG file pointer */
   png_structp   png_ptr;        /* internally used by libpng */
   png_infop     info_ptr;       /* user requested transforms */

   char         *image_data;      /* raw png image data */
   char         sig[8];           /* PNG signature array */
   /*char         **row_pointers;   */

   int           bit_depth;
   int           color_type;

   unsigned long width;            /* PNG image width in pixels */
   unsigned long height;           /* PNG image height in pixels */
   unsigned int rowbytes;         /* raw bytes at row n in image */

   image_data = NULL;
   int i;
   png_bytepp row_pointers = NULL;

   /* Open the file. */
   infile = fopen(file, "rb");
   if (!infile) {
      return 0;
   }


   /*
    * 		13.3 readpng_init()
    */
	
   /* Check for the 8-byte signature */
   fread(sig, 1, 8, infile);

   if (!png_check_sig((unsigned char *) sig, 8)) {
      fclose(infile);
      return 0;
   }
 
   /* 
    * Set up the PNG structs 
    */
   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (!png_ptr) {
      fclose(infile);
      return 4;    /* out of memory */
   }
 
   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr) {
      png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
      fclose(infile);
      return 4;    /* out of memory */
   }
   
  
  /*
   * block to handle libpng errors, 
   * then check whether the PNG file had a bKGD chunk
   */
   if (setjmp(png_jmpbuf(png_ptr))) {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      fclose(infile);
      return 0;
   }

  /* 
   * takes our file stream pointer (infile) and 
   * stores it in the png_ptr struct for later use.
   */
   /* png_ptr->io_ptr = (png_voidp)infile;*/
   png_init_io(png_ptr, infile);
   
  /*
   * lets libpng know that we already checked the 8 
   * signature bytes, so it should not expect to find 
   * them at the current file pointer location
   */
   png_set_sig_bytes(png_ptr, 8);
   
   /* Read the image info.*/

  /*
   * reads and processes not only the PNG file's IHDR chunk 
   * but also any other chunks up to the first IDAT 
   * (i.e., everything before the image data).
   */

   /* read all the info up to the image data  */
   png_read_info(png_ptr, info_ptr);

   png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, 
	&color_type, NULL, NULL, NULL);

   *pwidth = width;
   *pheight = height;
   
   /* Set up some transforms. */
   if (color_type & PNG_COLOR_MASK_ALPHA) {
      png_set_strip_alpha(png_ptr);
   }
   if (bit_depth > 8) {
      png_set_strip_16(png_ptr);
   }
   if (color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(png_ptr);
   }
   if (color_type == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(png_ptr);
   }
   
   /* Update the png info struct.*/
   png_read_update_info(png_ptr, info_ptr);

   /* Rowsize in bytes. */
   rowbytes = png_get_rowbytes(png_ptr, info_ptr);


   /* Allocate the image_data buffer. */
   if ((image_data = (unsigned char *) malloc(rowbytes * height))==NULL) {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 4;
    }

   if ((row_pointers = (png_bytepp)malloc(height*sizeof(png_bytep))) == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        free(image_data);
        image_data = NULL;
        return 4;
    }
   

    /* set the individual row_pointers to point at the correct offsets */

    for (i = 0;  i < height;  ++i)
        row_pointers[height - 1 - i] = image_data + i*rowbytes;


    /* now we can go ahead and just read the whole image */
    png_read_image(png_ptr, row_pointers);

    /* and we're done!  (png_read_end() can be omitted if no processing of
     * post-IDAT text/time/etc. is desired) */

   /* Clean up. */
   free(row_pointers);

   /* Clean up. */
   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
   fclose(infile);

   *image_data_ptr = image_data;

   return 1;
}

static const char *strnchr(const char *str, size_t len, char c)
{
   const char *e = str + len;
   do {
      if (*str == c) {
         return str;
      }
   } while (++str < e);
   return NULL;
}

int32_t render_subtitle(GRAPHICS_RESOURCE_HANDLE img, const char *text, const int skip, const uint32_t text_size, const uint32_t y_offset)
{
   uint32_t text_length = strlen(text)-skip;
   uint32_t width=0, height=0;
   const char *split = text;
   int32_t s=0;
   int len = 0; // length of pre-subtitle
   uint32_t img_w, img_h;

   graphics_get_resource_size(img, &img_w, &img_h);

   if (text_length==0)
      return 0;
   while (split[0]) {
      s = graphics_resource_text_dimensions_ext(img, split, text_length-(split-text), &width, &height, text_size);
      if (s != 0) return s;
      if (width > img_w) {
         const char *space = strnchr(split, text_length-(split-text), ' ');
         if (!space) {
            len = split+1-text;
            split = split+1;
         } else {
            len = space-text;
            split = space+1;
         }
      } else {
         break;
      }
   }
   // split now points to last line of text. split-text = length of initial text. text_length-(split-text) is length of last line
   if (width) {
      s = graphics_resource_render_text_ext(img, (img_w - width)>>1, y_offset-height,
                                     GRAPHICS_RESOURCE_WIDTH,
                                     GRAPHICS_RESOURCE_HEIGHT,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     split, text_length-(split-text), text_size);
      if (s!=0) return s;
   }
   return render_subtitle(img, text, skip+text_length-len, text_size, y_offset - height);
}
//------------------------------------------------------------------------------

static void exit_func(void)
// Function to be passed to atexit().
{
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   int s;
   // clear screen
   glClear( GL_COLOR_BUFFER_BIT );
   eglSwapBuffers(state->display, state->surface);

   glDeleteTextures(1, state->tex);
   eglDestroySurface( state->display, state->surface );

   dispman_update = vc_dispmanx_update_start( 0 );
   s = vc_dispmanx_element_remove(dispman_update, state->dispman_element);
   assert(s == 0);
   vc_dispmanx_update_submit_sync( dispman_update );
   s = vc_dispmanx_display_close(state->dispman_display);
   assert (s == 0);

   // Release OpenGL resources
   eglMakeCurrent( state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
   eglDestroyContext( state->display, state->context );
   eglTerminate( state->display );

   // release texture buffers
   free(state->tex_buf1);
   

   printf("\ncube closed\n");
} // exit_func()

//==============================================================================

int main ()
{
   bcm_host_init();
   memset( state, 0, sizeof( *state ) );
   init_ogl(state);
   init_model_proj(state);
   init_textures(state);
   
   GRAPHICS_RESOURCE_HANDLE img;
   uint32_t width, height;
   int LAYER=1;
   int s;
   s = gx_graphics_init(".");
   assert(s == 0);
   s = graphics_get_display_size(0, &width, &height);
   assert(s == 0);
   s = gx_create_window(0, width, height, GRAPHICS_RESOURCE_RGBA32, &img);
   assert(s == 0);
   // transparent before display to avoid screen flash
   graphics_resource_fill(img, 0, 0, width, height, GRAPHICS_RGBA32(0,0,0,0x00));
   graphics_display_resource(img, 0, LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);
   uint32_t text_size = 70;
   /* TEST RPM VALUES BEFORE HOOKING UP*/
   int values[] = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900, 5000, 5100, 5200, 5300, 5400, 5500, 5600, 5700, 5800, 5900, 6000, 6100, 6200, 6300, 6400, 6500, 6600, 6700, 6800, 6900, 7000, 6900, 6800, 6700, 6600, 6500, 6400, 6300, 6200, 6100, 6000, 5900, 5800, 5700, 5600, 5500, 5400, 5300, 5200, 5100, 5000, 5000, 5100, 5200, 5300, 5400, 5500, 5600, 5700, 5800, 5900, 6000, 6100, 6200, 6300, 6400, 6500, 6600, 6700, 6800, 6900, 7000, 6900, 6800, 6700, 6600, 6500, 6400, 6300, 6200, 6100, 6000, 5900, 5800, 5700, 5600, 5500, 5400, 5300, 5200, 5100, 5000, 4900, 4800, 4700, 4600, 4500, 4400, 4300, 4200, 4100, 4000, 3900, 3800, 3700, 3600, 3500, 3400, 3300, 3200, 3100, 3000, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900, 5000, 4900, 4800, 4700, 4600, 4500, 4400, 4300, 4200, 4100, 4000, 3900, 3800, 3700, 3600, 3500, 3400, 3300, 3200, 3100, 3000, 2900, 2800, 2700, 2600, 2500, 2400, 2300, 2200, 2100, 2000, 1900, 1800, 1700, 1600, 1500, 1400, 1300, 1200, 1100, 1000, 900, 800, 700, 600, 500, 400, 300, 200, 100, 0};
   int counter = 0;
   char text[16];

   while (!terminate)
   {
	   sprintf(text, "%d", values[counter]);

      uint32_t y_offset = height-60+text_size/2;
      graphics_resource_fill(img, 0, 0, width, height, GRAPHICS_RGBA32(0,0,0,0x00));
      render_subtitle(img, text, 0, text_size,  y_offset);
      graphics_update_displayed_resource(img, 0, 0, 0, 0);
      
      valint = values[counter];
      update_model(state);
      redraw_scene(state);
      counter++;
      
   }
   graphics_display_resource(img, 0, LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 0);
   graphics_delete_resource(img);
   exit_func();
   return 0;
}


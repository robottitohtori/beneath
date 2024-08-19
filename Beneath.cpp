// Beneath.cpp : Defines the entry point for the console application.#
//
#define _USE_MATH_DEFINES
#define GL_GLEXT_PROTOTYPES
#define EIGEN_MPL2_ONLY

#include "stdafx.h"
#include "GL/glew.h"
#include "GL/wglew.h"
#include "GL/freeglut.h"
#include "Eigen/Dense"
#include "Eigen/Geometry"
#include <iostream>
#include <cstdint>
#include <cmath>
#include "surf.h"
#include "curve.h"
#include "triglookup.h"
#include "MP3Player.h"
#include "geom.h"
#include "resource1.h"

using namespace Eigen;
using namespace std;

void updateAnims();

// general stuff
const bool showFPS = false;
bool fullscreen = true;

Vector3f light0InitPosition = Vector3f(1.f, 3.f, 2.5f);
Vector4f light0CurrentPosition = Vector4f(1.f, 3.f, 2.5f, 1.f);

Vector4f light1CurrentPosition = Vector4f(-3.f, 3.f, 3.f, 1.f);
Vector4f light2CurrentPosition = Vector4f(3.f, 3.f, 3.f, 1.f);

const float currentLookAtPos[3] = {0.f, 0.f, 10.f};
const float currentLookAtDir[3] = {0.f, 1.f, 0.f};
float col[4] = {1.0f, 0.f, 0.f, 0.f};
float col2[4] = {0.0f, 0.f, 1.f, 0.f};
const GLfloat specColor[] = {1.0, 1.0, 1.0, 1.0};
const GLfloat shininess[] = {100.0};
const GLfloat diff[] = {1.0f, 1.0f, 1.0f, 1.0f};
const GLfloat amb[] = {0.0f, 0.0f, 0.0f, 1.0f};

GLuint fbo, fbo_texture, rbo_depth, vbo_fbo_vertices;
GLuint program_postproc, attribute_v_coord_postproc, uniform_fbo_texture, uniform_offset, uniform_plasmaintensity, uniform_outputintensity;

TrigLookup<1024> trig;

// display lists
GLuint tunnelKnotDL;
GLuint tunnelKnotDLClipped;

// object-alikes
vector<float> heartRate; 
Surface heart;
Surface tunnelKnot;
objectMatrix cubes;
wordMatrix credits[7];

// anim-stuff (chaos..)
float heartScale = 1.f;
int timeOffset = 0;
bool tunnelKnotEnable = false;
bool tunnelKnotRotate = false;
float tunnelKnotSep = 0.f;
float tunnelKnotZDelta = 0.0034f;
float tunnelKnotSepAmount = 0.0088f;
bool tunnelKnotSepEnable = false;
bool heartEnable = false;
float tunnelKnotPosition = -97.f;
float tunnelKnotRot = 0.f;
float tunnelKnotSpacing = 5.f;
float tunnelKnotDst = -30.f;
unsigned int tunnelKnotCount = 20;
float plasmaIntensity = 1.f;
float plasmaIntensityDelta = .0005f;
int prevNow = 0;
int now = 0;
int nowDelta = 0;
bool outroFadeout = false;
bool moveHeartAndTunnelKnots = false;
float heartPosition = 0.f;
float outputIntensity = 1.f;
bool cubesEnable = false;
bool cubesRandomSignal = false;
int cubesRandomSignalMemory = 0;
bool zoomerEnable = false;
float outroFov = 60;
bool cubesRotationEnable = true;
const unsigned int gearsCount = 16;
bool gearsEnable = false;
vector<Vector3i> skull;
int outroFovDivisor = 70; //32
vector<wordBool> words;
float wordScale = 0.f;
bool creditsEnable = false;
float wordMargins[7] = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.05f};
bool wordEnable[7] = {false, false, false, false, false, false, false};
float wordScales[7] = {0.4f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
float cubesScale = 1.f;
bool cubesScaleEnable = false;

inline int getTime()
{
	return glutGet(GLUT_ELAPSED_TIME) - timeOffset;
}

void cleanup()
{
	glDeleteRenderbuffers(1, &rbo_depth);
	glDeleteTextures(1, &fbo_texture);
	glDeleteFramebuffers(1, &fbo);


	glDeleteBuffers(1, &vbo_fbo_vertices);


	glDeleteProgram(program_postproc);
}

void resizeWindow(int Width, int Height)
{
	glViewport(0, 0, Width, Height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float aspectRatio = (float)Width / (float)Height;
	gluPerspective(60.0, aspectRatio, 1.0, 100.0);

	
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, Width, Height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0); 

	//winWidth = Width;
	//winHeight = Height;
}

void updateAnims()
{
	prevNow = now;
	now = getTime();
	nowDelta = now - prevNow;

	// Light0 rot
	light0CurrentPosition.block<3, 1>(0,0) = AngleAxisf((float)now / 1500, Vector3f::UnitZ()) * light0InitPosition;

	// rotatorseparator
	if (tunnelKnotSepEnable)
	{
		if (tunnelKnotSep > 1.5f || tunnelKnotSep < 0.f)
			tunnelKnotSepAmount *= -1;
		tunnelKnotSep += tunnelKnotSepAmount * nowDelta / 44.f;
	}

	// shaderstuff
	if (outroFadeout)
	{
		outputIntensity -= (float)nowDelta / 3642;
	}
	glUniform1f(uniform_outputintensity, outputIntensity);
	glUniform1f(uniform_offset, (float)now / 1500);
	plasmaIntensity += nowDelta * plasmaIntensityDelta;
	glUniform1f(uniform_plasmaintensity, plasmaIntensity);


	// heartPulse
	if (heartEnable)
	{
		heartScale = 0.6f + 0.06f * interpolate(heartRate, (float)(now % 458)/458.f - 0.030f);
		updateSurface(heart, (float)(now % 15000)/15000);
	}

	// tunnelKnot
	if (tunnelKnotEnable && tunnelKnotPosition < tunnelKnotDst)
	{
		tunnelKnotPosition += nowDelta * tunnelKnotZDelta;
	}

	// tunnelvision
	if (moveHeartAndTunnelKnots)
	{
		tunnelKnotPosition += nowDelta * 0.035f;
		heartPosition += nowDelta * 0.035f;
		if (heartPosition > 15)
			heartEnable = false;
	}

	// cubes
	if (cubesEnable)
	{
		cubes.diaspora += 0.001f * trig.Sin(now * 15);
		cubes.update(now - 58247);
		
		if (cubesRandomSignal)
		{
			cubesRandomSignalMemory += nowDelta;
			if (cubesRandomSignalMemory > 114)
			{
				cubes.randomSignal(50);
				cubesRandomSignalMemory -= 114;
			}
		}

		if (cubesScaleEnable)
		{
			cubes.diaspora += (float)nowDelta / 256;
		}
	}

	// freakoutpart zoomer
	if (zoomerEnable)
	{
		outroFov -= (float)nowDelta / outroFovDivisor;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(outroFov, 1.777, 1.0, 100.0);
	}
}

void genHeartRate()
{
	heartRate.push_back(1.f);
	heartRate.push_back(0.7f);
	heartRate.push_back(0.5f);
	heartRate.push_back(0.4f);
	heartRate.push_back(0.3f);
	heartRate.push_back(0.2f);
	heartRate.push_back(0.15f);
	heartRate.push_back(0.1f);
	heartRate.push_back(0.2f);
	heartRate.push_back(0.5f);
}

void drawScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_LIGHTING);
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

	// Calculate the camera position and orientation.
	// Could probably optimize this, but let's keep it simple to read for now.
	//Vector3f currentLookAtPos = Matrix3f::rotateY(yRot) * Matrix3f::rotateX(xRot) * Matrix3f::uniformScaling(zoomLevel) * lookAtPos;
	//Vector3f currentLookAtDir = Matrix3f::rotateX(xRot) * lookAtDir;

    gluLookAt(currentLookAtPos[0], currentLookAtPos[1], currentLookAtPos[2],
			0, 0, 0,
			currentLookAtDir[0], currentLookAtDir[1], currentLookAtDir[2]);

    glLightfv(GL_LIGHT0, GL_POSITION, light0CurrentPosition.data());

	//float now = (float)getTime();

	if (heartEnable)
	{
		glPushMatrix();
		/*
		Matrix4f scaler;
		scaler = Scaling(Vector4f(heartScale, heartScale, heartScale, 1.f));
		Matrix4f transl = Matrix4f::Identity();
		transl(0, 3) = 6.f * trig.Sin(4 * getTime());
		transl(1, 3) = 3.f * trig.Sin(8 * getTime());
		transl(2, 3) = heartPosition; */
		Affine3f rot;
		//rot = AngleAxisf((now - 459)/4000.f, Vector3f::UnitX()) * AngleAxisf((now - 459)/3000.f, Vector3f::UnitX()) * AngleAxisf((now - 459)/2000.f, Vector3f::UnitZ());
		rot = AngleAxisf((now - 1200)/1714.f, Vector3f::UnitX()) * AngleAxisf((now - 1200)/2000.f, Vector3f::UnitZ());
		
		/*
		glMultMatrixf(transl.data());
		glMultMatrixf(rot.data());
		glMultMatrixf(scaler.data());
		*/
		glTranslatef(6.f * trig.Sin(4 * (now + 2000)), 3.f * trig.Sin(8 * (now + 2000)), heartPosition); //2650 //2919
		glMultMatrixf(rot.data());
		glScalef(heartScale, heartScale, heartScale);
		drawSurface(heart);
		glPopMatrix();
	}

	if (tunnelKnotEnable)
	{
		glPushMatrix();
		//Matrix4f scaler;
		//scaler = Scaling(Vector4f(1.2f, 1.2f, 1.2f, 1.f));
		//glMultMatrixf(scaler.data());
		Matrix4f transl = Matrix4f::Identity();
		Affine3f rot;
		if (tunnelKnotRotate)
			rot = AngleAxisf((now - 29391)/500.f - 0.09f, Vector3f::UnitZ()); // 28932
		else
			rot = AngleAxisf(-0.09f, Vector3f::UnitZ());
		//transl.block<3, 3>(0, 0) = .matrix();
		for (unsigned int x = 0; x < 2; x++)
		{
			transl(0, 3) = tunnelKnotSep * (-5.f + (float)x * 10.f);
			for (unsigned int y = 0; y < 2; y++)
			{

				transl(1, 3) = tunnelKnotSep * (-5.f + (float)y * 10.f);
				for (unsigned int i = 0; i < tunnelKnotCount; i++)
				{
	
					transl(2, 3) = tunnelKnotPosition - i * tunnelKnotSpacing;
					if (transl(2, 3) < 10.f  && transl(2, 3) > -100.f)
					{
						glPushMatrix();
						glMultMatrixf((transl * rot).data());
						//glMultMatrixf(rot.data());
						glCallList(tunnelKnotDL);
						glPopMatrix();
					}
				}
			}
		}
		glPopMatrix();
	}

	if (cubesEnable)
	{
		glPushMatrix();
		glTranslatef(0.f, 0.f, -12.f - 3 * cubes.diaspora);
		if (cubesRotationEnable)
		{
			//glRotatef(30.f * trig.Sin(((float)getTime() - 58247) * 0.01228166f), 0.f, 1.f, 0.f);
			glRotatef(30.f * trig.Sin(now * 8.94275f), 0.f, 1.f, 0.f);
			glRotatef(30.f * trig.Cos(now * 17.8855f), 1.f, 0.f, 0.f);
		}
		glScalef(0.6f, 0.6f, 0.6f);
		// 1832
		cubes.draw();
		glPopMatrix();
	}

	if (gearsEnable)
	{
		glPushMatrix();
		glScalef(1.1f, 1.1f, 1.1f);
		for (unsigned int i = 0; i < gearsCount; i++)
		{
			glPushMatrix();
			//glTranslatef(gearLocations[i](0), gearLocations[i](1), 5 * trig.Sin(now * i) + gearLocations[i](2));
			//glRotatef((1 + (float)i / 10) * now / 30, 0.f, 0.f, 1.f);
			glRotatef(now / 30.f + i * 360.f / gearsCount, 0.f, 0.f, 1.f);
			//glRotatef((1 + (float)i / 10) * now / 50, 1.f, 0.f, 0.f);
			//glRotatef((1 + (float)i / 10) * now / 40, 0.f, 1.f, 0.f);
			//float gcol[4] = {
			//glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);
			glCallList(tunnelKnotDLClipped);
			glPopMatrix();
		}
		glPopMatrix();
	}

	if (creditsEnable)
	{
		for (unsigned int i = 0; i < 7; i++)
		{
			if (wordEnable[i])
			{
				glPushMatrix();
				//float scale = 0.05f * trig.Sin(143 * now) + ;
				glTranslatef(0.f, 0, -10.f + 2 * wordMargins[i]);
				glScalef(wordScales[i], wordScales[i], wordScales[i]);
				(i == 0) ? glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col2) : glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);
				credits[i].draw(wordMargins[i], 1.f + 0.2f * trig.Sin(143 * now));
				glPopMatrix();
				(i == 0) ? wordMargins[i] += 0.0010f * nowDelta / 16.f + 0.02f * wordMargins[i] * wordMargins[i] : wordMargins[i] += 0.0020f * nowDelta / 16.f + 0.045f * wordMargins[i] * wordMargins[i];
			}
		}
	}

	if (showFPS)
	{
		glDisable(GL_LIGHTING);
		glRasterPos2f(-9.5f, -5.5f);
		glColor3f(0.5f, 0.5f, 0.5f);
		char temp[8];
		sprintf(temp, "%u", nowDelta);
		glutBitmapString(GLUT_BITMAP_TIMES_ROMAN_24, (unsigned char*)temp);
	}
		
}

void drawBuffers()
{
	updateAnims();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glUseProgram(0);
	drawScene();
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glUseProgram(program_postproc);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glUniform1i(uniform_fbo_texture, 0);

	glEnableVertexAttribArray(attribute_v_coord_postproc);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_fbo_vertices);
	glVertexAttribPointer(
		attribute_v_coord_postproc,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		0
	);
	
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(attribute_v_coord_postproc);
	
	glutSwapBuffers();
}

void toggleFullscreen()
{
	/*fullscreen = !fullscreen;
	if (fullscreen)
		glutFullScreen(); */
	glutFullScreenToggle();
}

void keyboardFunc( unsigned char key, int x, int y )
{
    if (key == 27)
		glutLeaveMainLoop();
	else if (key == 'f' || key == 'F')
		toggleFullscreen();
}

void initGL()
{
	
	//s// 
	//glutInitContextVersion(4, 0);
	glutInitContextFlags(GLUT_FORWARD_COMPATIBLE);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	//s// 

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
	glutInitWindowPosition(0, 0);
    glutInitWindowSize(1920, 1080);
    glutCreateWindow("Beneath");
	if (fullscreen)
		glutFullScreen();

	glutDisplayFunc(drawBuffers);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(60.0, 1.777, 1.0, 100.0);

	glutReshapeFunc(resizeWindow);
	glutKeyboardFunc(keyboardFunc);

	// glew //

	GLenum GlewInitResult = glewInit();
	if (GlewInitResult != GLEW_OK) 
	{
		cerr << "Error: (GlewInit) " << glewGetErrorString(GlewInitResult) << endl;
		exit(1);
	}

	cout << "Got GL_VERSION " << glGetString(GL_VERSION) << endl;
	
	// /GLEW //

	glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.f);
	glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.f);
	glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.f);
		//GL_LIGHT0, GL_CONSTANT_ATTENUATION
		//GL_QUADRATIC_ATTENUATION

	gluLookAt(currentLookAtPos[0], currentLookAtPos[1], currentLookAtPos[2],
		0, 0, 0,
		currentLookAtDir[0], currentLookAtDir[1], currentLookAtDir[2]);

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);

    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specColor);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

	//GLfloat amb[] = {1.0, 1.0, 1.0, 1.0};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
	glLightfv(GL_LIGHT0, GL_AMBIENT, amb);

	return;
}

void initBuffers()
{
	unsigned int w = 1920;
	unsigned int h = 1080;
	GLfloat fbo_vertices[] = 
	{
		-1, -1,
		1, -1,
		-1,  1,
		1,  1,
	};
	glGenBuffers(1, &vbo_fbo_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_fbo_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(fbo_vertices), fbo_vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);


		//s//
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &fbo_texture);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Depth buffer //
	glGenRenderbuffers(1, &rbo_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	// Framebuffer to link everything together //
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
	GLenum status;
	if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) 
	{
		cerr << "Error: (glCheckFramebufferStatus)" << status << endl;
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
} 

GLuint createShader(const string& source, GLenum type) 
{

	GLuint res = glCreateShader(type);
	const GLchar* sources[] = {
#ifdef GL_ES_VERSION_2_0
		"#version 100\n"  // OpenGL ES 2.0
#else
		"#version 120\n"  // OpenGL 2.1
#endif
	, source.c_str() };
	glShaderSource(res, 2, sources, NULL);
		
	glCompileShader(res);
	GLint compile_ok = GL_FALSE;
	glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok == GL_FALSE) {
		cerr << "Error: (create_shader) " << res << endl;;
		glDeleteShader(res);
		return 0;
	}
	
	return res;
} 

bool initShaders()
{
	string vertexShader("attribute vec2 v_coord;\
uniform sampler2D fbo_texture;\
varying vec2 f_texcoord;\
void main(void) {\
gl_Position = vec4(v_coord, 0.0, 1.0);\
f_texcoord = (v_coord + 1.0) / 2.0;\
}");


// should get x and y offsets properly..
string fragShader("uniform sampler2D fbo_texture;\
uniform float offset;\
uniform float plasmaintensity;\
uniform float outputintensity;\
varying vec2 f_texcoord;\
float rand(vec2 co){\
    return fract(sin(offset + dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);\
}\
void main(void) {\
	float offsetx = 1.2 / 1920.0;\
	float offsety = 1.2 / 1080.0;\
	vec2 offsets[25] = vec2[](\
	vec2(-2 * offsetx, 2 * offsety),\
	vec2(-offsetx, 2 * offsety),\
	vec2(0.0f, 2 * offsety),\
	vec2(offsetx, 2 * offsety),\
	vec2(2 * offsetx, 2 * offsety),\
	vec2(-2 * offsetx, offsety),\
	vec2(-offsetx, offsety),\
	vec2(0.0f, offsety),\
	vec2(offsetx, offsety),\
	vec2(2 * offsetx, offsety),\
	vec2(-2 * offsetx, 0.f),\
	vec2(-offsetx, 0.f),\
	vec2(0.0f, 0.f),\
	vec2(offsetx, 0.f),\
	vec2(2 * offsetx, 0.f),\
	vec2(-2 * offsetx, -offsety),\
	vec2(-offsetx, -offsety),\
	vec2(0.0f, -offsety),\
	vec2(offsetx, -offsety),\
	vec2(2 * offsetx, -offsety),\
	vec2(-2 * offsetx, -2 * offsety),\
	vec2(-offsetx, -2 * offsety),\
	vec2(0.0f, -2 * offsety),\
	vec2(offsetx, -2 * offsety),\
	vec2(2 * offsetx, -2 * offsety)\
	);\
	float a = 0.12f, b = 0.06f;\
	float kernel[25] = float[](\
		b, b, b, b, b,\
		b, a, a, a, b,\
		b, a, 1.0, a, b,\
		b, a, a, a, b,\
		b, b, b, b, b\
	);\
	vec3 col = vec3(0.0);\
	for(int i = 0; i < 25; i++)\
	{\
	col += kernel[i] * pow(vec3(texture2D(fbo_texture, f_texcoord + offsets[i])), vec3(2, 2, 2));\
	}\
	float rnd = rand(f_texcoord);\
	float mult = plasmaintensity * (2.f - 1.5f * f_texcoord.x) * (1.f - f_texcoord.y) / (2000 * length(col) + 1);\
	float plasma = mult * ( 0.23f * pow(sin(13 * sin(offset * 0.4571f) * f_texcoord.y + 11 * f_texcoord.x + offset * 2.5f), 2) + 0.21f * pow(sin(7 * f_texcoord.y + 17 * f_texcoord.x + 2 * sin(offset * 0.5217f) + offset * 1.6846f), 2) + 0.19f * pow(sin(8 * f_texcoord.y + 15 * f_texcoord.x + 1.5 * sin(offset * 0.7617f) + offset * 1.8246f), 2) );\
	rnd *= 1 + plasma;\
	col.x +=  (0.20f + 0.14f * sin(offset/2)) * rnd;\
	col.y +=  (0.15f + 0.10f * sin(offset/3.5)) * rnd;\
	col.z +=  (0.17f + 0.12f * sin(offset/4.5)) * rnd;\
	col *= 0.383f;\
	col *= outputintensity;\
	gl_FragColor = clamp(vec4(col, 1.0f), 0.f, 1.f);\
}");

	// Post-processing //
	GLuint vs, fs;
	GLint link_ok, validate_ok;
	if ((vs = createShader(vertexShader, GL_VERTEX_SHADER))   == 0)
	{
		cerr << "Error: (initShaders) vertex shader" << endl;
		return false;
	}
	if ((fs = createShader(fragShader, GL_FRAGMENT_SHADER)) == 0)
	{
		cerr << "Error: (initShaders) fragment shader" << endl;
		return false;
	}

	program_postproc = glCreateProgram();
	glAttachShader(program_postproc, vs);
	glAttachShader(program_postproc, fs);
	glLinkProgram(program_postproc);
	glGetProgramiv(program_postproc, GL_LINK_STATUS, &link_ok);
	if (!link_ok) 
	{
		cerr << "Error: (glLinkProgram)" << endl;
		return false;
	}
	glValidateProgram(program_postproc);
	glGetProgramiv(program_postproc, GL_VALIDATE_STATUS, &validate_ok); 
	if (!validate_ok) 
	{
		cerr << "Error: (glValidateProgram)" << endl;
		return false;
	}

	string attribute_name = "v_coord";
	attribute_v_coord_postproc = glGetAttribLocation(program_postproc, attribute_name.c_str());
	if (attribute_v_coord_postproc == -1) 
	{
		cerr << "Error: (initShaders) Attribute binding failed: " << attribute_name << endl;
		return false;
	}

	string uniform_name = "fbo_texture";
	uniform_fbo_texture = glGetUniformLocation(program_postproc, uniform_name.c_str());
	if (uniform_fbo_texture == -1) 
	{
		cerr << "Error: (initShaders) Uniform binding failed: " << uniform_name << endl;
	    return false;
	}


	uniform_name = "offset";
	uniform_offset = glGetUniformLocation(program_postproc, uniform_name.c_str());
	if (uniform_offset == -1) 
	{
		cerr << "Error: (initShaders) Uniform binding failed: " << uniform_name << endl;
	    return false;
	}
	
	uniform_name = "plasmaintensity";
	uniform_plasmaintensity = glGetUniformLocation(program_postproc, uniform_name.c_str());
	if (uniform_plasmaintensity == -1) 
	{
		cerr << "Error: (initShaders) Uniform binding failed: " << uniform_name << endl;
	    return false;
	}

	uniform_name = "outputintensity";
	uniform_outputintensity = glGetUniformLocation(program_postproc, uniform_name.c_str());
	if (uniform_outputintensity == -1) 
	{
		cerr << "Error: (initShaders) Uniform binding failed: " << uniform_name << endl;
	    return false;
	}
	
	return true;
} 

void pushSkull(vector<Vector3i>& v)
{
	v.reserve(28);
	Vector3i offs = Vector3i(-1, 0, 0);
	v.push_back(Vector3i(12, 8, 0) + offs);
	v.push_back(Vector3i(13, 8, 0) + offs);
	v.push_back(Vector3i(14, 8, 0) + offs);
	v.push_back(Vector3i(15, 8, 0) + offs);

	v.push_back(Vector3i(11, 9, 0) + offs);
	v.push_back(Vector3i(16, 9, 0) + offs);

	v.push_back(Vector3i(10, 10, 0) + offs);
	v.push_back(Vector3i(12, 10, 0) + offs);
	v.push_back(Vector3i(15, 10, 0) + offs);
	v.push_back(Vector3i(17, 10, 0) + offs);

	v.push_back(Vector3i(9, 11, 0) + offs);
	v.push_back(Vector3i(18, 11, 0) + offs);

	v.push_back(Vector3i(10, 12, 0) + offs);
	v.push_back(Vector3i(11, 12, 0) + offs);
	v.push_back(Vector3i(16, 12, 0) + offs);
	v.push_back(Vector3i(17, 12, 0) + offs);

	v.push_back(Vector3i(12, 13, 0) + offs);
	v.push_back(Vector3i(15, 13, 0) + offs);

	v.push_back(Vector3i(12, 14, 0) + offs);
	v.push_back(Vector3i(13, 14, 0) + offs);
	v.push_back(Vector3i(14, 14, 0) + offs);
	v.push_back(Vector3i(15, 14, 0) + offs);

	v.push_back(Vector3i(12, 16, 0) + offs);
	v.push_back(Vector3i(15, 16, 0) + offs);

	v.push_back(Vector3i(12, 17, 0) + offs);
	v.push_back(Vector3i(13, 17, 0) + offs);
	v.push_back(Vector3i(14, 17, 0) + offs);
	v.push_back(Vector3i(15, 17, 0) + offs);

}

void pushGearLocations(vector<Vector3f>& v)
{
	v.reserve(40);
	float z = -60.f;
	for (int i = 0; i < 40; i++)
	{
		float x = (((float)rand() / RAND_MAX) - 0.5f) * 177;
		float y = (((float)rand() / RAND_MAX) - 0.5f) * 100;
		v.push_back(Vector3f(x, y, z));
	}
	/*
	v.push_back(Vector3f(-50.f, 25.f, z));
	v.push_back(Vector3f(-46.f, -12.f, z));
	v.push_back(Vector3f(-31.f, -28.f, z));
	v.push_back(Vector3f(-17.f, 33.f, z));
	v.push_back(Vector3f(-0.f, 19.f, z));
	v.push_back(Vector3f(5.f, -22.f, z));
	v.push_back(Vector3f(28.f, 5.f, z));
	v.push_back(Vector3f(33.f, 8.f, z));
	v.push_back(Vector3f(45.f, 10.f, z));
	v.push_back(Vector3f(50.f, -5.f, -z));

	v.push_back(Vector3f(-80.f, 30.f, z));
	v.push_back(Vector3f(-76.f, -26.f, z));
	v.push_back(Vector3f(-61.f, -35.f, z));
	v.push_back(Vector3f(-57.f, 44.f, z));
	v.push_back(Vector3f(-68.f, 34.f, z));
	v.push_back(Vector3f(72.f, -12.f, z));
	v.push_back(Vector3f(57.f, 25.f, z));
	v.push_back(Vector3f(64.f, 31.f, z));
	v.push_back(Vector3f(78.f, 10.f, z));
	v.push_back(Vector3f(86.f, -25.f, z));

	v.push_back(Vector3f(26.f, -26.f, z));
	v.push_back(Vector3f(49.f, -23.f, z));
	v.push_back(Vector3f(-39.f, -22.f, z));
	v.push_back(Vector3f(39.f, -45.f, z));
	v.push_back(Vector3f(-67.f, -37.f, z));

	v.push_back(Vector3f(0.f, -35.f, z));
	v.push_back(Vector3f(4.f, 23.f, z));
	v.push_back(Vector3f(-6.f, -19.f, z));
	v.push_back(Vector3f(-18.f, 0.f, z));
	v.push_back(Vector3f(11.f, -3.f, z));
	*/
}

void pushHeartProfilePoints(vector<Vector3f>& v)
{
	v.reserve(12);
	float s = 0.5f;
	v.push_back(s * Vector3f(0.f, 1.f, 0.f));
	v.push_back(s * Vector3f(-0.2f, 0.8f, 0.f));
	v.push_back(s * Vector3f(-0.5f, 0.2f, 0.f));
	v.push_back(s * Vector3f(-0.1f, 0.1f, 0.f));
	v.push_back(s * Vector3f(-0.5f, -0.2f, 0.f));
	v.push_back(s * Vector3f(0.f, -1.f, 0.f));
	v.push_back(s * Vector3f(0.3f, -0.5f, 0.f));
	v.push_back(s * Vector3f(0.3f, 0.3f, 0.f));
	v.push_back(s * Vector3f(0.4f, 0.9f, 0.f));
	v.push_back(s * Vector3f(0.f, 1.f, 0.f));
	v.push_back(s * Vector3f(-0.2f, 0.8f, 0.f));
	v.push_back(s * Vector3f(-0.5f, 0.2f, 0.f));
}

void pushHeartScalePoints(vector<Vector3f>& v)
{
	v.reserve(25);
	v.push_back(Vector3f(0.f, 0.25f, 0.f));
	v.push_back(Vector3f(0.f, 0.25f, 0.f));
	v.push_back(Vector3f(0.f, 4.55f, 0.f));
	v.push_back(Vector3f(0.f, 2.55f, 0.f));
	v.push_back(Vector3f(0.f, 0.45f, 0.f));
	v.push_back(Vector3f(0.f, 1.2f, 0.f));
	v.push_back(Vector3f(0.f, 2.25f, 0.f));
	v.push_back(Vector3f(0.f, 0.35f, 0.f));
	v.push_back(Vector3f(0.f, 0.25f, 0.f));
	v.push_back(Vector3f(0.f, 5.25f, 0.f));

	v.push_back(Vector3f(0.f, 1.2f, 0.f));
	v.push_back(Vector3f(0.f, 2.25f, 0.f));
	v.push_back(Vector3f(0.f, 0.35f, 0.f));
	v.push_back(Vector3f(0.f, 0.25f, 0.f));
	v.push_back(Vector3f(0.f, 5.25f, 0.f));

	v.push_back(Vector3f(0.f, 0.25f, 0.f));
	v.push_back(Vector3f(0.f, 4.95f, 0.f));
	v.push_back(Vector3f(0.f, 0.55f, 0.f));
	v.push_back(Vector3f(0.f, 2.45f, 0.f));
	v.push_back(Vector3f(0.f, 0.2f, 0.f));
	v.push_back(Vector3f(0.f, 2.75f, 0.f));
	v.push_back(Vector3f(0.f, 1.35f, 0.f));
	v.push_back(Vector3f(0.f, 1.85f, 0.f));
	v.push_back(Vector3f(0.f, 0.25f, 0.f));
	v.push_back(Vector3f(0.f, 0.25f, 0.f));
	v.push_back(Vector3f(0.f, 4.55f, 0.f));
}

void pushTunnelKnotProfilePoints(vector<Vector3f>& v)
{
	float s = 0.5f;
	float a = 1.0f;
	float b = 0.25f;
	float n = 0.f;
	v.reserve(11);
	v.push_back(s * Vector3f(n, a, n));
	v.push_back(s * Vector3f(-b, b, n));
	v.push_back(s * Vector3f(-a, n, n));
	v.push_back(s * Vector3f(-b, -b, n));
	v.push_back(s * Vector3f(n, -a, n));
	v.push_back(s * Vector3f(b, -b, n));
	v.push_back(s * Vector3f(a, n, n));
	v.push_back(s * Vector3f(b, b, n));
	v.push_back(s * Vector3f(n, a, n));
	v.push_back(s * Vector3f(-b, b, n));
	v.push_back(s * Vector3f(-a, n, n));
}

void createObjects()
{
	// pulseHeart
	vector<Vector3f> heartProfilePoints;
	vector<Vector3f> heartScalePoints;
	vector<Vector3f> tunnelKnotProfilePoints;
	pushHeartProfilePoints(heartProfilePoints);
	pushHeartScalePoints(heartScalePoints);
	SurfaceOptions opts;
	opts.enableNormalColoring = true;
	opts.enableProfileScaleCurve = true;
	opts.profileScaleCurve = getCurveYVec(evalBspline(heartScalePoints, 10)); // 30
	heart = makeGenCyl(evalBspline(heartProfilePoints, 10), evalTrefoilKnot(1.5f, 100, &trig), opts); // 30 100
	genHeartRate();

	// tunnelknot
	pushTunnelKnotProfilePoints(tunnelKnotProfilePoints);
	tunnelKnot = makeGenCyl(evalBspline(tunnelKnotProfilePoints, 10), evalTrefoilKnot(6.f, 100, &trig)); // 10 100
	
	// cubes
	cubes = objectMatrix(20, 20, 20, 1.4f, 1.4f, 1.4f, drawUnitCube, &trig);
	pushSkull(skull);

	// credits
	for (unsigned int i = 0; i < 7; i++)
		credits[i] = wordMatrix(words[i]);

	return;
}

void reDraw()
{
	glutPostRedisplay();
}

void eventEnableCredits(int i)
{
	creditsEnable = true;
	wordEnable[0] = true;
}

void eventEnableCredits1(int i)
{
	wordEnable[1] = true;
}

void eventEnableCredits2(int i)
{
	wordEnable[2] = true;
}

void eventEnableCredits3(int i)
{
	wordEnable[3] = true;
	wordEnable[1] = false;
}

void eventEnableCredits4(int i)
{
	wordEnable[4] = true;
	wordEnable[2] = false;
}

void eventEnableCredits5(int i)
{
	wordEnable[5] = true;
	wordEnable[3] = false;
}

void eventEnableCredits6(int i)
{
	wordEnable[6] = true;
	wordEnable[4] = false;
}

void eventDisableCredits(int i)
{
	creditsEnable = false;
}

void eventEnableTunnelKnot(int i)
{
	tunnelKnotEnable = true;
	wordEnable[5] = false;
}

void eventTunnelKnotSep(int i)
{
	tunnelKnotSepEnable = true;
}

void eventEnableTunnelKnotRotate(int i)
{
	tunnelKnotRotate = true;
	heartEnable = true;
	//gluPerspective(60.0, 1.777, 1.0, 100.0);

	glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.03f);
	glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.03f);
	glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.03f);

    GLfloat diff1[] = {0.7, 1.0, 0.7, 1.0};
	GLfloat diff2[] = {0.7, 0.7, 1.0, 1.0};
	GLfloat amb[] = {0.0, 0.0, 0.0, 1.0};

    glLightfv(GL_LIGHT1, GL_DIFFUSE, diff1);
	glLightfv(GL_LIGHT1, GL_AMBIENT, amb);

	glLightfv(GL_LIGHT2, GL_DIFFUSE, diff2);
	glLightfv(GL_LIGHT2, GL_AMBIENT, amb);

	glLightfv(GL_LIGHT1, GL_POSITION, light1CurrentPosition.data());

	glLightfv(GL_LIGHT2, GL_POSITION, light2CurrentPosition.data());

	glEnable(GL_LIGHT1);
	glEnable(GL_LIGHT2);
}

void eventGetTimeOffset(int i)
{
	timeOffset = glutGet(GLUT_ELAPSED_TIME);
	cout << "got timeOffset " << timeOffset << endl;
}

void eventStopDemo(int i)
{
	glutLeaveMainLoop();
}

void eventOutroFadeout(int i)
{
	outroFadeout = true;
	cubesRandomSignal = false;
	cubes.staticSignal(skull);
}

void eventMoveHeartAndTunnelKnots(int i)
{
	moveHeartAndTunnelKnots = true;
}

void eventOutro(int i)
{
	cubes = objectMatrix(25, 25, 1, 1.4f, 1.4f, 1.4f, drawUnitCube, &trig);
	cubesRandomSignal = true;
	cubesRotationEnable = false;
	cubes.modulateScale = false;
	zoomerEnable = false;
	gearsEnable = false;
	cubesScaleEnable = false;
	cubesScale = 1.0f;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(20, 1.777, 1.0, 100.0);
	glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.01f);
	glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.01f);
	glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.01f);
	light1CurrentPosition = Vector4f(-3.f, 3.f, 5.f, 1.f);
	light2CurrentPosition = Vector4f(3.f, 3.f, 5.f, 1.f);
}

void eventZoomer(int i)
{
	zoomerEnable = true;
}

void eventZoomerDivisor(int i)
{
	outroFovDivisor = -20;
	cubesScaleEnable = true;
	gearsEnable = false;
}

void eventPlasmaBoxen(int i)
{
	tunnelKnotEnable = false;
	heartEnable = false;
	cubesEnable = true;
	gearsEnable = true;
	glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.015f);
	glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.015f);
	glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.015f);
	light0InitPosition = Vector3f(1.f, 3.f, 0.f);
	light1CurrentPosition = Vector4f(-3.f, 3.f, -10.f, 1.f);
	light2CurrentPosition = Vector4f(3.f, 3.f, -10.f, 1.f);
}

void eventStart(int i)
{
	//gearsEnable = true;
	//cubesEnable = true;
	//cubes.colorLock = skull;
	//cubes.staticSignal(skull);
}

void registerEvents()
{

	unsigned int extra = 459;
	unsigned int extra2 = 58;
	glutTimerFunc(0, eventStart, 1);

	glutTimerFunc(extra2, eventEnableCredits, 1);
	glutTimerFunc(extra2 + 4 * 916, eventEnableCredits1, 1);
	glutTimerFunc(extra2 + 6 * 916, eventEnableCredits2, 1);
	glutTimerFunc(extra2 + 8 * 916, eventEnableCredits3, 1);
	glutTimerFunc(extra2 + 10 * 916, eventEnableCredits4, 1);
	glutTimerFunc(extra2 + 12 * 916, eventEnableCredits5, 1);
	glutTimerFunc(extra2 + 14 * 916, eventEnableCredits6, 1);
	glutTimerFunc(extra + 14000, eventEnableTunnelKnot, 1); //14257
	glutTimerFunc(extra + 16500, eventDisableCredits, 1);
	glutTimerFunc(extra + 28932, eventEnableTunnelKnotRotate, 1);
	glutTimerFunc(extra + 25272, eventTunnelKnotSep, 1);
	glutTimerFunc(extra + 54583, eventMoveHeartAndTunnelKnots, 1);
	glutTimerFunc(extra + 58247, eventPlasmaBoxen, 1);
	glutTimerFunc(extra + 87533, eventZoomer, 1);
	glutTimerFunc(extra + 91203, eventZoomerDivisor, 1);
	glutTimerFunc(extra + 94873, eventOutro, 1);
	glutTimerFunc(extra + 109546, eventOutroFadeout, 1);
	glutTimerFunc(extra + 113188, eventStopDemo, 1);
	// 54583 nostatus melodiaan
	// 58247 melodia
	// 87533 nostatus loppuun
	// 94873 outro
	// 109546 loppufade
}

void createDisplayLists()
{
	// tunnelKnot
	tunnelKnotDL = glGenLists(1); 
	if (tunnelKnotDL == 0)
	{
		// Error in list creation. 
		cout << "Error: glGenLists(1) returned 0. " << endl;   
		exit(1);
	}
	glNewList(tunnelKnotDL, GL_COMPILE); 
	// Push the triangles to the list
	glBegin(GL_TRIANGLES);
	drawSurface(tunnelKnot);
	glEnd();
	glEndList();


	// tunnelKnot Cliped
	tunnelKnotDLClipped = glGenLists(1); 
	if (tunnelKnotDLClipped == 0)
	{
		// Error in list creation. 
		cout << "Error: glGenLists(1) returned 0. " << endl;   
		exit(1);
	}
	glNewList(tunnelKnotDLClipped, GL_COMPILE); 
	// Push the triangles to the list
	glBegin(GL_TRIANGLES);
	drawSurface(tunnelKnot, 8.f);
	glEnd();
	glEndList();
}

void processBitfield(byte* field, unsigned int rows, unsigned int cols, unsigned int wordCount, unsigned int *wordBoundaries, vector<wordBool> &result)
{
	//unsigned int wordCount = wordBoundaries.size() / 2;
	for (unsigned int w = 0; w < wordCount; w++)
	{
		unsigned int width = wordBoundaries[2 * w + 1] - wordBoundaries[2 * w] + 1;
		unsigned int start = wordBoundaries[2 * w];
		wordBool wb = wordBool(width, rows);
		for (unsigned int y = 0; y < rows; y++)
		{
			for (unsigned int x = 0; x < width; x++)
			{
				unsigned int bi = (y * cols + start + x) % 8;
				unsigned int by = (y * cols + start + x) / 8;
				wb.set(x, y, (bool)(field[by] & 1 << (7 - bi)));
			}
		}
		result.push_back(wb);
		//cout << wb.word.data() << endl;
	}
}

void getVSync()
{
	typedef void (APIENTRY * PFNWGLEXTSWAPCONTROLPROC) (int i);
	PFNWGLEXTSWAPCONTROLPROC wglSwapControl = NULL;
	wglSwapControl = (PFNWGLEXTSWAPCONTROLPROC) wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapControl != NULL) {
		wglSwapControl(1); 
		cout << "Got Vsync. " << endl;
	}
}

void loadResource(unsigned int res, LPVOID &lpAddress, DWORD &size)
{
	HMODULE hModule = GetModuleHandle(NULL);
	HRSRC hResource = FindResource(hModule, MAKEINTRESOURCE(res), RT_RCDATA);
	HGLOBAL hMemory = LoadResource(hModule, hResource);
	size = SizeofResource(hModule, hResource);
	lpAddress = LockResource(hMemory);
}

int main(int argc, char* argv[])
{
	// Should we take over?
	//if (argc > 1)
	for (unsigned int i = 1; i < argc; i++)
	{
		if (argv[i] == string("-nofullscreen"))
			fullscreen = false;
		//else if (argv[i] == string("-partylights"))
		//	partyLightsEnable = true;
	}

	// Load resources
	DWORD musicSize, creditsSize;
	LPVOID lpAddress;
	loadResource(IDR_RCDATA1, lpAddress, musicSize);
	byte *music = new byte[musicSize];
	memcpy(music, lpAddress, musicSize);
	loadResource(IDR_RCDATA2, lpAddress, creditsSize);
	byte *credits = new byte[creditsSize];
	memcpy(credits, lpAddress, creditsSize);

	// Process credits bitmap (this is braindead)
	unsigned int wordBoundaries[] = {0, 39, 46, 79, 87, 107, 115, 130, 138, 164, 172, 182, 189, 255};
	processBitfield(credits, 9, 256, 7, wordBoundaries, words);

	// Jumpstart player
	MP3Player player;
	player.OpenFromMemory(music, musicSize);
	
	// Inits
	glutInit(&argc, argv);
	initGL();
	glutIdleFunc(reDraw);
	glutSetCursor(GLUT_CURSOR_NONE);
	initBuffers();
	initShaders();
	getVSync();

	// Objects
	createObjects();
	createDisplayLists();

	// Lets go!
	cout << "Starting..." << endl;
	player.Play();
	timeOffset = glutGet(GLUT_ELAPSED_TIME);
	registerEvents();

	glutMainLoop();

	// And lets go..
	cout << "Exiting. " << endl;
	player.Close();
	delete[] music;
	delete[] credits;
	cleanup();

	return 0;
}
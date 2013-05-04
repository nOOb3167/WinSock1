#include <cstdlib>
#include <cassert>
#include <iostream>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <oglplus/all.hpp>

using namespace oglplus;

void nothing(void)
{
	oglplus::Context gl;
	gl.ClearColor(1.0f, 0.0f, 0.0f, 0.0f);
	gl.Clear().ColorBuffer();

	glutSwapBuffers();
}		

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
	glutInitContextVersion (3,3);
	glutInitContextProfile (GLUT_CORE_PROFILE);
	glewExperimental=TRUE;
	glutCreateWindow("OGLplus");
	assert (glewInit() == GLEW_OK);
	glGetError();

	glutDisplayFunc(nothing);
	glutMainLoop();

	return EXIT_SUCCESS;
}

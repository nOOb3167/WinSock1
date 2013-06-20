#include <cstdlib>
#include <cassert>

#include <memory>
#include <vector>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <oglplus/all.hpp>

using namespace std;
using namespace oglplus;

class SaneStoreState {
public:
	GLint swapbytes, lsbfirst, rowlength, skiprows, skippixels, alignment;

	SaneStoreState() {
		glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swapbytes);
		glGetIntegerv(GL_UNPACK_LSB_FIRST, &lsbfirst);
		glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlength);
		glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows);
		glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippixels);
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);

		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	~SaneStoreState() {
		glPixelStorei(GL_UNPACK_SWAP_BYTES, swapbytes);
		glPixelStorei(GL_UNPACK_LSB_FIRST, lsbfirst);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlength);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippixels);
		glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
	}
};

GLubyte * CreateImageBufRGB(const char *fname, int *w, int *h) {
	int r;
	HBITMAP hBmp;
	HDC hdc;
	int tmpW, tmpH;
	GLubyte *buf;

	assert(w && h);

	hBmp = (HBITMAP)LoadImageA(NULL, fname, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	assert(hBmp);

	hdc = CreateCompatibleDC(NULL);
	assert(hdc);

	BITMAPINFO bInfo = {0};
	bInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	r = GetDIBits(hdc, hBmp, 0, 0, NULL, &bInfo, DIB_RGB_COLORS);
	assert(r);

	/* Only interested in Width and Height, now prime the structure for the 2nd GetDIBits call.
	   http://msdn.microsoft.com/en-us/library/windows/desktop/dd183376%28v=vs.85%29.aspx
	       Says if biBitCount = 32 && biCompression = BI_RGB then bmiColors will be empty. */
	tmpW = bInfo.bmiHeader.biWidth;
	tmpH = bInfo.bmiHeader.biHeight;

	assert(bInfo.bmiHeader.biPlanes == 1);
	assert(bInfo.bmiHeader.biBitCount == 32);
	bInfo.bmiHeader.biCompression = BI_RGB;
	bInfo.bmiHeader.biSizeImage = 0;
	bInfo.bmiHeader.biClrUsed = 0;
	bInfo.bmiHeader.biClrImportant = 0;

	buf = new GLubyte[bInfo.bmiHeader.biWidth * bInfo.bmiHeader.biHeight * 4];
	assert (buf);

	r = GetDIBits(hdc, hBmp, 0, bInfo.bmiHeader.biHeight, buf, &bInfo, DIB_RGB_COLORS);
	assert(r);

	r = DeleteDC(hdc);
	assert(r);

	r = DeleteObject(hBmp);
	assert(r);

	*w = tmpW;
	*h = tmpH;

	return buf;
};

oglplus::Texture CreateTexture(const char *fname) {
	int w, h;
	SaneStoreState sss;

	unique_ptr<GLubyte[]> buf(CreateImageBufRGB(fname, &w, &h));
	shared_ptr<oglplus::images::Image> im = make_shared<oglplus::images::Image>(w, h, 1, 4, buf.get());

	oglplus::Texture tex;
	tex.Active(0);
	tex.Bind(oglplus::TextureTarget::_2D);
	tex.MinFilter(oglplus::TextureTarget::_2D, oglplus::TextureMinFilter::Nearest);
	tex.MagFilter(oglplus::TextureTarget::_2D, oglplus::TextureMagFilter::Nearest);
	tex.Image2D(oglplus::TextureTarget::_2D, *im);
	tex.Unbind(oglplus::TextureTarget::_2D);

	return tex;
}

class Ex1 {
public:
	aiScene *scene;

	oglplus::Texture tex;

	Ex1() {
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\mCube01.dae", aiProcessPreset_TargetRealtime_MaxQuality));
		assert(scene);

		tex = CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp");
	}

	void Display() {
	}
};

static Ex1 *gEx = nullptr;

void display(void) {
	oglplus::Context gl;
	gl.ClearColor(1.0f, 0.0f, 0.0f, 0.0f);
	gl.Clear().ColorBuffer().DepthBuffer();

	assert(gEx);
	gEx->Display();

	glutSwapBuffers();
}

int main(int argc, char **argv) {

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

	gEx = new Ex1();

	glutDisplayFunc(display);
	glutMainLoop();

	return EXIT_SUCCESS;
}

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

void Demo() {
	int r;
	HDC hdc;
	
	HBITMAP hBmp = (HBITMAP)LoadImageA(NULL, "C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	assert(hBmp);

	hdc = CreateCompatibleDC(NULL);
	assert(hdc);

	BITMAPINFO *bI2 = (BITMAPINFO *)calloc(1, sizeof *bI2);
	bI2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	r = GetDIBits(hdc, hBmp, 0, 0, NULL, bI2, DIB_RGB_COLORS);
	assert(r);
}

GLubyte * CreateImageBufRGB(const char *fname, int *w, int *h) {
	int r;
	HBITMAP hBmp;
	HDC hdc;
	DWORD dwBmpSize;
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

	/* Conservatively assume 4 components, 32 bpp, rows aligned to pixel boundary. */
	assert(bInfo.bmiHeader.biBitCount == 32);
	dwBmpSize = ((bInfo.bmiHeader.biWidth * bInfo.bmiHeader.biBitCount + 31) / 32) * 4 * bInfo.bmiHeader.biHeight;

	buf = new GLubyte[dwBmpSize];
	assert (buf);

	r = GetDIBits(hdc, hBmp, 0, bInfo.bmiHeader.biHeight, buf, &bInfo, DIB_RGB_COLORS);
	assert(r);

	r = DeleteDC(hdc);
	assert(r);

	r = DeleteObject(hBmp);
	assert(r);

	*w = bInfo.bmiHeader.biWidth;
	*h = bInfo.bmiHeader.biHeight;

	return buf;
};

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

oglplus::Texture CreateTexture(const char *fname) {
	int w, h;
	SaneStoreState sss;

	unique_ptr<GLubyte[]> buf(CreateImageBufRGB(fname, &w, &h));
	shared_ptr<oglplus::images::Image> im = make_shared<oglplus::images::Image>(w, h, 1, 3, buf.get());

	oglplus::Texture tex;
	tex.Active(0);
	tex.Bind(oglplus::TextureTarget::_2D);
	tex.MinFilter(oglplus::TextureTarget::_2D, oglplus::TextureMinFilter::Nearest);
	tex.MagFilter(oglplus::TextureTarget::_2D, oglplus::TextureMagFilter::Nearest);
	tex.Image2D(oglplus::TextureTarget::_2D, *im);

	return tex;
}

int main() {
	aiLogStream stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT,NULL);
	aiScene *scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\mCube01.dae", aiProcessPreset_TargetRealtime_MaxQuality));
	assert(scene);

	Demo();

	oglplus::Texture tex = CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp");

	return EXIT_SUCCESS;
}

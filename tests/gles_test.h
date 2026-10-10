#ifndef NUVIO_GLES_TEST_H
#define NUVIO_GLES_TEST_H
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* GLES2 has no unpack row-length setting. SDL surfaces may have padding. */
static void gles_test_upload_surface(const SDL_Surface *surface) {
  const void *pixels = surface->pixels;
  unsigned char *packed = NULL;
  size_t row = (size_t)surface->w * 4;
  if ((size_t)surface->pitch != row) {
    packed = malloc(row * surface->h);
    assert(packed);
    for (int y = 0; y < surface->h; ++y)
      memcpy(packed + row * y, (const char *)pixels + surface->pitch * y, row);
    pixels = packed;
  }
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  free(packed);
}

/* Read texture texels through an FBO; GLES2 has no glGetTexImage. */
static void gles_test_read_texture(GLuint texture, int w, int h, void *pixels) {
  GLint previous;
  GLuint fbo;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous);
  glDeleteFramebuffers(1, &fbo);
}
#endif

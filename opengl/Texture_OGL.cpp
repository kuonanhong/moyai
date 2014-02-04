#ifdef USE_OPENGL

#include "Texture_OGL.h"

#include "../soil/src/SOIL.h"

bool Texture_OGL::load( const char *path ){
	tex = SOIL_load_OGL_texture( path,
		SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID,
		SOIL_FLAG_MULTIPLY_ALPHA );
	if(tex==0)return false;
	glBindTexture( GL_TEXTURE_2D, tex );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );     
	print("soil_load_ogl_texture: new texid:%d", tex );

	// generate image from gl texture
	if(image) delete image;
	image = new Image();
	int w,h;
	getSize(&w,&h);
	image->setSize(w,h); 
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->buffer );

	return true;
}

void Texture_OGL::setLinearMagFilter(){
	glBindTexture( GL_TEXTURE_2D, tex );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
}

void Texture_OGL::setLinearMinFilter(){
	glBindTexture( GL_TEXTURE_2D, tex );    
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );    
}

void Texture_OGL::setImage( Image *img ) {
	if(tex==0){
		glGenTextures( 1, &tex );
		assertmsg(tex!=0,"glGenTexture failed");
		glBindTexture( GL_TEXTURE_2D, tex );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST ); 

	}

	glBindTexture(GL_TEXTURE_2D, tex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->buffer );

	image = img;
}

#endif
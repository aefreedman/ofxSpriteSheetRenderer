/***********************************************************************
 
 Copyright (C) 2011 by Zach Gage
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 ************************************************************************/ 

#include "CollageTexture.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

CollageTexture::CollageTexture(const CollageTexture & other)
    : ofTexture(other), c_width(other.c_width), c_height(other.c_height),
      c_bpp(other.c_bpp), c_type(other.c_type), internal(other.internal) {
    if (other.collage) {
        const size_t size = size_t(c_width) * c_height * c_bpp;
        collage = new unsigned char[size];
        std::copy(other.collage, other.collage + size, collage);
    }
}

CollageTexture::CollageTexture(CollageTexture && other) noexcept
    : ofTexture(std::move(other)), collage(other.collage), c_width(other.c_width),
      c_height(other.c_height), c_bpp(other.c_bpp), c_type(other.c_type), internal(other.internal) {
    other.collage = nullptr;
    other.c_width = other.c_height = other.c_bpp = other.c_type = 0;
}

CollageTexture & CollageTexture::operator=(const CollageTexture & other) {
    if (this != &other) {
        CollageTexture copy(other);
        *this = std::move(copy);
    }
    return *this;
}

CollageTexture & CollageTexture::operator=(CollageTexture && other) noexcept {
    if (this != &other) {
        ofTexture::operator=(std::move(other));
        delete[] collage;
        collage = other.collage;
        c_width = other.c_width;
        c_height = other.c_height;
        c_bpp = other.c_bpp;
        c_type = other.c_type;
        internal = other.internal;
        other.collage = nullptr;
        other.c_width = other.c_height = other.c_bpp = other.c_type = 0;
    }
    return *this;
}

CollageTexture::~CollageTexture()
{
	delete[] collage;
}

void CollageTexture::allocate(int w, int h, int internalGlDataType, int internalGLScaleMode){
	allocate(w, h, internalGlDataType, internalGLScaleMode, ofGetUsingArbTex());
}

void CollageTexture::allocate(int w, int h, int internalGlDataType, int internalGLScaleMode, bool bUseARBExtention){
	
    // Invalid requests leave the existing CPU buffer and GL ownership intact.
    const int bpp = internalGlDataType == GL_RGB ? 3 : internalGlDataType == GL_RGBA ? 4 : 0;
    const int maxDimension = 1 << 30; // ofNextPow2 must remain representable as int.
    if (w <= 0 || h <= 0 || w > maxDimension || h > maxDimension || !bpp ||
        size_t(w) > size_t(std::numeric_limits<int>::max()) / size_t(h) / size_t(bpp)) {
        return;
    }
    const size_t size = size_t(w) * h * bpp;
    unsigned char * replacement = new (std::nothrow) unsigned char[size]();
    if (!replacement) return;
    delete[] collage;
    collage = replacement;
    c_width = w;
    c_height = h;
    c_bpp = bpp;

	c_type = internalGlDataType;
	
	//our graphics card might not support arb so we have to see if it is supported.
#ifndef TARGET_OPENGLES
	if (bUseARBExtention && GL_ARB_texture_rectangle){
		texData.tex_w = w;
		texData.tex_h = h;
		texData.tex_t = w;
		texData.tex_u = h;
		texData.textureTarget = GL_TEXTURE_RECTANGLE_ARB;
	} else 
#endif
	{
		//otherwise we need to calculate the next power of 2 for the requested dimensions
		//ie (320x240) becomes (512x256)
		texData.tex_w = ofNextPow2(w);
		texData.tex_h = ofNextPow2(h);
		texData.tex_t = 1.0f;
		texData.tex_u = 1.0f;
		texData.textureTarget = GL_TEXTURE_2D;
	}
	
	texData.glInternalFormat = internalGlDataType;
	
	
	// MEMO: todo, add more types
	switch(texData.glInternalFormat) {
#ifndef TARGET_OPENGLES	
		case GL_RGBA32F_ARB:
		case GL_RGBA16F_ARB:
			internal.glType		= GL_RGBA;
			internal.pixelType = GL_FLOAT;
			break;
			
		case GL_RGB32F_ARB:
			internal.glType = GL_RGB;
			internal.pixelType = GL_FLOAT;
			break;
			
		case GL_LUMINANCE32F_ARB:
			internal.glType = GL_LUMINANCE;
			internal.pixelType = GL_FLOAT;
			break;
#endif			
			
		default:
			internal.glType = GL_LUMINANCE;
			internal.pixelType = GL_UNSIGNED_BYTE;
	}
	
    // Allocate the legacy padded size explicitly: OF may otherwise choose NPOT
    // storage. Its supported allocator registers the ID for shared ownership.
    const float textureT = texData.tex_t;
    const float textureU = texData.tex_u;
    ofTexture::allocate(int(texData.tex_w), int(texData.tex_h), internalGlDataType,
                        bUseARBExtention, internalGlDataType, GL_UNSIGNED_BYTE);
    texData.tex_t = textureT;
    texData.tex_u = textureU;

	glEnable(texData.textureTarget);
	
	glBindTexture(texData.textureTarget, (GLuint)texData.textureID);
	
	// Filtering and wrapping use different enum domains. Preserve the effective
	// defaults left by the old invalid wrap calls: repeat for 2D, clamp for ARB.
	GLint wrapMode = GL_REPEAT;
#ifndef TARGET_OPENGLES
	if (texData.textureTarget == GL_TEXTURE_RECTANGLE_ARB) {
		wrapMode = GL_CLAMP_TO_EDGE;
	}
#endif
	texData.minFilter = internalGLScaleMode;
	texData.magFilter = internalGLScaleMode;
	texData.wrapModeHorizontal = wrapMode;
	texData.wrapModeVertical = wrapMode;
	glTexParameterf(texData.textureTarget, GL_TEXTURE_MAG_FILTER, texData.magFilter);
	glTexParameterf(texData.textureTarget, GL_TEXTURE_MIN_FILTER, texData.minFilter);
	glTexParameterf(texData.textureTarget, GL_TEXTURE_WRAP_S, texData.wrapModeHorizontal);
	glTexParameterf(texData.textureTarget, GL_TEXTURE_WRAP_T, texData.wrapModeVertical);
	
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	glDisable(texData.textureTarget);
	
	texData.width = w;
	texData.height = h;
	texData.bFlipTexture = false;
	texData.bAllocated = true;
}

void CollageTexture::pasteImage(int x, int y, string textureName, int glType)
{
	ofImage loader;
	loader.setUseTexture(false);
	if (!loader.load(textureName)) return;
	
	pasteImage(x, y, loader.getWidth(), loader.getHeight(), loader.getPixels().getData(), glType);
	
	loader.clear();
}

void CollageTexture::pasteImage(int x, int y, int w, int h, unsigned char * pxls, int glType)
{	
	int bpp = 3;
	
	if(glType == GL_RGB)
		bpp=3;
	else if(glType == GL_RGBA)
		bpp=4;
	
	
	int collageIndex = getIndex(x,y);
	int sourceIndex = 0;
	int sourceRow = 0;
	
	int collageMax = c_width*c_height*c_bpp;
	int sourceMax = w*h*bpp;
	
	int startX = x;
		
	while(1)
	{
		
		if(x >= c_width || x-startX >= w) // if we're passed the edge of the collage or done with a row of the image, go to the next row
		{
			x = startX;
			y++;
			sourceRow++;
			sourceIndex = (sourceRow * w)*bpp;
			collageIndex = getIndex(x,y);
			
			if(collageIndex >= collageMax) //if we're passed the edge of the collage, break!
				break;
		}
		
		if(sourceIndex >= sourceMax) // have we managed to go past the end of any files?
			break;
		
		//if we're still good, copy in the pixels
		
		if(c_bpp == 4) // do we blend?
		{
			if( collage[collageIndex + 3] != 0) // something non transparent exists as this pixel, therefore: blend!
			{
				float dest_pct = (float)collage[collageIndex + 3]/255.0f;
				
				float source_pct  = 1;
				if(bpp==4)
					source_pct = (float)pxls[sourceIndex + 3]/255.0f;
				
				//additive blend
				collage[collageIndex    ] = (float)collage[collageIndex    ] * (1.0-source_pct) + (float)pxls[sourceIndex    ] * source_pct;
				collage[collageIndex + 1] = (float)collage[collageIndex + 1] * (1.0-source_pct) + (float)pxls[sourceIndex + 1] * source_pct;
				collage[collageIndex + 2] = (float)collage[collageIndex + 2] * (1.0-source_pct) + (float)pxls[sourceIndex + 2] * source_pct;
				
				collage[collageIndex + 3] = 255 * (dest_pct * (1.0-source_pct) + source_pct );
			}
			else // don't blend! (blending with nothing ends up diluting the color
			{
				collage[collageIndex    ] = pxls[sourceIndex    ];
				collage[collageIndex + 1] = pxls[sourceIndex + 1];
				collage[collageIndex + 2] = pxls[sourceIndex + 2];
				if(bpp==4)
					collage[collageIndex + 3] = pxls[sourceIndex + 3];
				else
					collage[collageIndex + 3] = 255;
			}
			
		}
		else // just copy straight, theres no blend data being stored
		{
			collage[collageIndex    ] = pxls[sourceIndex    ];
			collage[collageIndex + 1] = pxls[sourceIndex + 1];
			collage[collageIndex + 2] = pxls[sourceIndex + 2];
		}
		
		x++;
		collageIndex+=c_bpp;
		sourceIndex+=bpp;
	}
}

int CollageTexture::getIndex(int x, int y)
{
	return (y*c_width+x)*c_bpp;
}

void CollageTexture::finish()
{
	loadData(collage, c_width, c_height, c_type);
	//delete[] collage;
	//clear collage
	for(int i=0;i<c_width*c_height*c_bpp;i++)
		collage[i]=0;
}

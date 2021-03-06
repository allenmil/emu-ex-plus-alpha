#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <engine-globals.h>
#include <resource2/font/ResourceFont.h>
#include <resource2/font/common/glyphTable.h>
#define RESOURCE_FACE_SETTINGS_UNCHANGED 128
#include <io/sys.hh>
#include <pixmap/Pixmap.hh>
#include <data-type/image/GfxImageSource.hh>

class ResourceFace
{
public:
	FontSettings settings;
	static constexpr bool supportsUnicode = Config::UNICODE_CHARS;

	constexpr ResourceFace() {}
	static ResourceFace *create(ResourceFont *font, FontSettings *set = nullptr);
	static ResourceFace *create(ResourceFace *face, FontSettings *set = nullptr);
	static ResourceFace *load(const char *path, FontSettings *set = nullptr);
	static ResourceFace *load(Io* io, FontSettings *set = nullptr);
	static ResourceFace *loadAsset(const char *name, FontSettings *set = nullptr)
	{
		return load(openAppAssetIo(name), set);
	}
	static ResourceFace *loadSystem(FontSettings *set = nullptr);
	void free();
	CallResult applySettings(FontSettings set);
	//int maxDescender();
	//int maxAscender();
	CallResult writeCurrentChar(IG::Pixmap &out);
	CallResult precache(const char *string);
	CallResult precacheAlphaNum()
	{
		return precache("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	}
	GlyphEntry *glyphEntry(int c);
	uint nominalHeight() const;

private:
	ResourceFont *font = nullptr;
	GlyphEntry *glyphTable = nullptr;
	FontSizeRef faceSize;
	uint nominalHeight_ = 0;

	void calcNominalHeight();
	void initGlyphTable ();
	CallResult cacheChar (int c, int tableIdx);
};

class GfxGlyphImage : public GfxImageSource
{
public:
	ResourceFace *face;
	GlyphEntry *entry;

	GfxGlyphImage(ResourceFace *face, GlyphEntry *entry): face(face), entry(entry) {}
	CallResult getImage(IG::Pixmap &dest) override { return face->writeCurrentChar(dest); }
	uint width() override { return entry->metrics.xSize; }
	uint height() override { return entry->metrics.ySize; }
	const PixelFormatDesc *pixelFormat() override { return &PixelFormatA8; }
};

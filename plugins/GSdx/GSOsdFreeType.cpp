/*
 *	Copyright (C) 2014-2014 Gregory hainaut
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "GSOsdFreeType.h"

void OsdManager::compute_glyph_size() {
	memset(c_info, 0, sizeof(c_info));
	atlas_w = 0;
	atlas_h = 0;

	for(uint32 i = ascii_start; i < ascii_stop; i++) {
		if(FT_Load_Char(face, i, FT_LOAD_RENDER))
			continue;

		// Size of char
		c_info[i].ax = face->glyph->advance.x >> 6;
		c_info[i].ay = face->glyph->advance.y >> 6;

		c_info[i].bw = face->glyph->bitmap.width;
		c_info[i].bh = face->glyph->bitmap.rows;

		c_info[i].bl = face->glyph->bitmap_left;
		c_info[i].bt = face->glyph->bitmap_top;

		c_info[i].tx = atlas_w;

		// Size of atlas
		atlas_w += face->glyph->bitmap.width;
		atlas_h = std::max(atlas_h, (uint32)face->glyph->bitmap.rows);
	}
}

OsdManager::OsdManager() : atlas_h(0), atlas_w(0), ascii_start(32), ascii_stop(128) {
	FT_Error error;
	error = FT_Init_FreeType(&library);
	if (error) {
		fprintf(stderr, "Failed to init the freetype library\n");
		return;
	}

	error = FT_New_Face(library, "/usr/share/fonts/truetype/freefont/FreeMono.ttf", 0, &face);
	if (error == FT_Err_Unknown_File_Format) {
		fprintf(stderr, "Failed to init the freetype face: unknwon format\n");
		return;
	} else if (error) {
		fprintf(stderr, "Failed to init the freetype face\n");
		return;
	}

	error = FT_Set_Pixel_Sizes(face, 0, 48*2);;
	if (error) {
		fprintf(stderr, "Failed to init the face size\n");
		return;
	}

	compute_glyph_size();
}

OsdManager::~OsdManager() {
	FT_Done_FreeType(library);
}

GSVector2i OsdManager::get_texture_font_size() {
	return GSVector2i(atlas_w, atlas_h);
}

void OsdManager::upload_texture_atlas(GSTexture* t) {
	for(uint32 i = ascii_start; i < ascii_stop; i++) {
		if(FT_Load_Char(face, i, FT_LOAD_RENDER)) {
			fprintf(stderr, "failed to load char '%c' aka %d\n", i, i);
			continue;
		}

		GSVector4i r(c_info[i].tx, 0, c_info[i].tx+c_info[i].bw, c_info[i].bh);
		if (r.width())
			t->Update(r, face->glyph->bitmap.buffer, c_info[i].bw);

	}
}

void OsdManager::text_to_vertex(GSVertexPT1* dst, const char* text, GSVector4 r)
{
	uint32 n = 0;
	for (const char* p = text; *p; p++) {
		float x2 =  r.x + c_info[*p].bl * r.z;
		float y2 = -r.y - c_info[*p].bt * r.w;
		float w = c_info[*p].bw * r.z;
		float h = c_info[*p].bh * r.w;

		/* Advance the cursor to the start of the next character */
		r.x += c_info[*p].ax * r.z;
		r.y += c_info[*p].ay * r.w;

		// NOTE: In the future we could use only a SHORT for texture
		// coordinate. And do the division by the texture size on the vertex
		// shader (need to drop dx9 compatibility)
		dst[n++] = (GSVertexPT1){GSVector4(x2    , -y2    , 0.0f, 0.0f)
                            ,GSVector2((float)c_info[*p].tx / (float)atlas_w                   , 0.0f                                 )};
		dst[n++] = (GSVertexPT1){GSVector4(x2 + w, -y2    , 0.0f, 0.0f)
                            ,GSVector2((float)(c_info[*p].tx + c_info[*p].bw) / (float)atlas_w , 0.0f                                 )};
		dst[n++] = (GSVertexPT1){GSVector4(x2    , -y2 - h, 0.0f, 0.0f)
                            ,GSVector2((float)c_info[*p].tx / atlas_w                          , (float)c_info[*p].bh / (float)atlas_h)};
		dst[n++] = (GSVertexPT1){GSVector4(x2 + w, -y2    , 0.0f, 0.0f)
                            ,GSVector2((float)(c_info[*p].tx + c_info[*p].bw) / (float)atlas_w , 0.0f                                 )};
		dst[n++] = (GSVertexPT1){GSVector4(x2    , -y2 - h, 0.0f, 0.0f)
                            ,GSVector2((float)c_info[*p].tx / atlas_w                          , (float)c_info[*p].bh / (float)atlas_h)};
		dst[n++] = (GSVertexPT1){GSVector4(x2 + w, -y2 - h, 0.0f, 0.0f)
                            ,GSVector2((float)(c_info[*p].tx + c_info[*p].bw) / (float)atlas_w , (float)c_info[*p].bh / (float)atlas_h)};
	}
}

#ifndef RENDER_PRIV_H
#define RENDER_PRIV_H

extern int remap_features;

Boolean refresh_palette(void *udata);
int find_supported_modes(unsigned dst_mode);

struct remap_object {
  struct remap_calls *calls;
  void *priv;
};

struct remap_object *remap_init(int src_mode, int dst_mode, int features,
        const ColorSpaceDesc *color_space);
void remap_done(struct remap_object *ro);
void remap_adjust_gamma(struct remap_object *ro, unsigned gamma);
int remap_palette_update(struct remap_object *ro, unsigned i,
	unsigned bits, unsigned r, unsigned g, unsigned b);
void remap_dst_resize(struct remap_object *ro, int width, int height,
	int scan_len);
RectArea remap_remap_rect(struct remap_object *ro,
	const struct bitmap_desc src_img,
	int x0, int y0, int width, int height,
	unsigned char *dst_img);
RectArea remap_remap_rect_dst(struct remap_object *ro,
	const struct bitmap_desc src_img,
	int x0, int y0, int width, int height, unsigned char *dst_img);
RectArea remap_remap_mem(struct remap_object *ro,
	const struct bitmap_desc src_img,
	unsigned src_start,
	unsigned dst_start, int offset, int len,
	unsigned char *dst_img);
int remap_get_cap(struct remap_object *ro);

#endif

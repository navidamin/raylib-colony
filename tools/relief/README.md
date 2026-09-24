# tools/relief — the Moon's real relief for the district level

`build_relief.py` builds `data/relief/`: the height tiles the district
level (200 km) is drawn from. What they are and why:
`docs/design/site-selection/level2-relief.md`; how the game reads them:
`src/TerrainGen/relief.h`.

```bash
pip install numpy pandas pyarrow rasterio scipy pillow
cd tools/relief
# the four catalogues, once (USGS Astrogeology ARD bucket)
for c in kaguya_terrain_camera_usgs_dtms_v2_equi kaguya_terrain_camera_usgs_dtms \
         kaguya_terrain_camera_usgs_dtms_v2_northpolar kaguya_terrain_camera_usgs_dtms_v2_southpolar; do
  curl -sO https://astrogeo-ard.s3-us-west-2.amazonaws.com/collectionindices/$c.parquet
done
export GDAL_DISABLE_READDIR_ON_OPEN=EMPTY_DIR CPL_VSIL_CURL_ALLOWED_EXTENSIONS=.tif \
       GDAL_HTTP_MAX_RETRY=5 GDAL_HTTP_RETRY_DELAY=2 GDAL_CACHEMAX=256
python3 -u build_relief.py            # every band, 86 N .. 86 S
python3 -u build_relief.py 6 2        # or just the bands whose tops are 6 N and 2 N
cp tiles/*.jpg ../../data/relief/
```

It reads each DTM straight from the bucket (Cloud-Optimized GeoTIFF
overviews, ~240 m), so nothing is downloaded whole. About 5 minutes a
4° band with 32 fetches in flight; four bands side by side ran the
whole Moon in about an hour. `done/` marks finished bands, so a stopped
build resumes. It needs the repo's LOLA model
(`src/assets/planet/lola/ldem_16_uint.tif`): the tiles store height
*above* its bilinear sample, and the game rebuilds the same base.

Each band prints how many DTMs it used and dropped, and the spread of
their disagreement with LOLA. Look at a district over any rebuilt band
before committing: a DTM that is wrong and alone shows as a rectangle.

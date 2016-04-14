# MDenoise:  Feature-Preserving Mesh Denoising

mdenoise is a simple and fast mesh denoising method, which can remove
noise effectively while preserving mesh features such as sharp edges and
corners. It can be applied to all kinds of point clouds, including
topographic data.

For further information see the [Cardiff University Mesh Filtering
Group](http://www.cs.cf.ac.uk/meshfiltering/index_files/Page342.htm) website.


### Usage

```
mdenoise -i input_file [options]
    -e         Common Edge Type of Face Neighbourhood (Default: Common Vertex)
    -t float   Threshold (0,1), Default value: 0.4
    -n int     Number of Iterations for Normal updating, Default value: 20
    -v int     Number of Iterations for Vertex updating, Default value: 50
    -o char[]  Output file
    -a         Adds edges and vertices to generate high-quality triangle mesh.
               Only functions when the input is .xyz file.
    -z         Only z-direction position is updated.
```
      
Examples:

+ `Mdenoise -i cylinderN02.ply2`
+ `Mdenoise -i cylinderN02.ply2 -n 5 -o cylinderDN`
+ `Mdenoise -i cylinderN02.ply2 -t 0.8 -e -v 20 -o cylinderDN.obj`
+ `Mdenoise -i FandiskNI02-05 -o FandiskDN.ply`
+ `Mdenoise -i Terrain.xyz -o TerrainP -z -n 1`
+ `Mdenoise -i my_dem_utm.asc -o my_dem_utmP -n 4`

##### About the file formats

Many standard CAD and geometrical processing file formats are supported.  The .xyz and .asc (ESRI ASCII Grid) files are primarily designed for dealing with geographic data. 

Supported input types: .gts, .obj, .off, .ply, .ply2, .smf, .stl, .wrl, .xyz, and .asc
Supported output types: .obj, .off, .ply, .ply2, .xyz, and .asc
Default file extension: .off

**Notes:**

+  .xyz files contain comma separated values, with x, y, z in the first three
   columns.  Optional comment lines (starting with #) at the beginning of the
   file and data in other columns are ignored.
+  The -z flag is always used for gridded topographic data (.asc files).  If an associated .prj file exists, a copy is provided for output file.


### Using mdenoise with topographic data

mdenoise denoises topographic data, particularly Shuttle Radar Topography
Mission (SRTM) data, with minimal changes to the data themselves.  It has
been implemented as a plugin in the
[GRASS](https://grass.osgeo.org/grass64/manuals/addons/r.denoise.html) and [SAGA](http://www.saga-gis.org/saga_module_doc/2.1.4/grid_filter_10.html) open source GIS packages.  See the [Using Sun's denoising algorithm on topographic data](http://personalpages.manchester.ac.uk/staff/neil.mitchell/mdenoise/) webpage for further information.


### References

The mdenoise code and its application to topographic data are described in
the following publications:

+  Sun X, Rosin PL, Martin RR, Langbein FC (2007) Fast and Effective Feature-Preserving Mesh Denoising. _IEEE Transactions on Visualization and Computer Graphics_, 13:925–938. doi: [10.1109/TVCG.2007.1065](http://dx.doi.org/10.1109/TVCG.2007.1065)
+  Stevenson JA, Sun X, Mitchell NC (2010) Despeckling SRTM and other topographic data with a denoising algorithm. _Geomorphology_ 114:238–252. doi: [10.1016/j.geomorph.2009.07.006](http://dx.doi.org/10.1016/j.geomorph.2009.07.006)


### Compilation instructions

To compile on unix platforms:

```
g++ -o mdenoise mdenoise.cpp triangle.c
```

Windows binaries are available from the [Cardiff University Mesh Filtering
Group](http://www.cs.cf.ac.uk/meshfiltering/index_files/Page342.htm) website.

--------

Copyright (C) 2007 Cardiff University, UK

Author: Xianfang Sun

Version: 1.0

**Note:**  The copyright of triangle.h and triangle.c belong to Jonathan Richard Shewchuk.  See the [TRIANGLE](http://people.sc.fsu.edu/~jburkardt/c_src/triangle/triangle.html) website and files themselves for further information.



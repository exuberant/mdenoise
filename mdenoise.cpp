/* MDenoise.cpp: Feature-Preserving Mesh Denoising.
 * Copyright (C) 2007 Cardiff University, UK
 *
 * Version: 1.0
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Su
 *
 * The copyright of triangle.h and triangle.c belong to Jonathan Richard Shewchuk
 * Further information about these two files see the files themselves.
 *
 * Author: Xianfang Sun
 *
 * usage: Mdenoise -i input_file [options]
 *      -e         Common Edge Type of Face Neighbourhood (Default: Common Vertex)
 *      -t float   Threshold (0,1), Default value: 0.4
 *      -n int     Number of Iterations for Normal updating, Default value: 20
 *      -v int     Number of Iterations for Vertex updating, Default value: 50
 *      -o char[]  Output file
 *      -a         Adds edges and vertices to generate high-quality triangle mesh.
 *                 Only function when the input is .xyz file.
 *      -z         Only z-direction position is updated.
 *
 * Supported input type: .gts, .obj, .off, .ply, .ply2, .smf, .stl, .wrl, .xyz, and .asc
 * Supported output type: .obj, .off, .ply, .ply2, .xyz, and .asc
 * Default file extension: .off
 *
 * Examples:
 * Mdenoise -i cylinderN02.ply2
 * Mdenoise -i cylinderN02.ply2 -n 5 -o cylinderDN
 * Mdenoise -i cylinderN02.ply2 -t 0.8 -e -v 20 -o cylinderDN.obj
 * Mdenoise -i FandiskNI02-05 -o FandiskDN.ply
 * Mdenoise -i -i Terrain.xyz -o TerrainP -z -n 1
 * Mdenoise -i my_dem_utm.asc -o my_dem_utmP -n 4
 *
 * Note: For the .asc file, the program always sets the switch -z on, whether you have 
 * put it on the command line or not. If there is a .prj file with the same name as the 
 * input .asc file, it is copied to a .prj file with the same name as the output .asc file. 
 * 
 * About the file formats:
 * All kinds of input files except for .xyz and .asc files have the standard format of
 * CAD or geometrical processing files. The .xyz and .asc files are primarily designed 
 * for dealing with geological data, where the .asc files have the standard ESRI ASCII Grid
 * format, and the .xyz files have the following format:
 * The first few lines (optional) are comment lines starting with #, and the rest lines are
 * the main data. Each line represents one point, consisting of the x, y, z coordinates, 
 * and (optional) other data. The program will only load and save x, y, z coordinates and
 * ignore all the other information.
 * 
 * Reference:
 * @article{SRML071,
 *   author = "Xianfang Sun and Paul L. Rosin and Ralph R. Martin and Frank C. Langbein",
 *   title = "Fast and effective feature-preserving mesh denoising",
 *   journal = "IEEE Transactions on Visualization and Computer Graphics",
 *   volume = "13",
 *   number = "5",
 *   pages  = "925--938",
 *   year = "2007",
 * }
 *
 * to compile on unix platforms:
 *     g++ -o mdenoise mdenoise.cpp triangle.c
 * also lines 66 & 112 of mdenoise.cpp should be commented out on unix platforms
 */

#define VOID int

#include "mdenoise.h"
#include "triangle.h"
#include <time.h>
//#include <new.h> // This line should be commented out on unix

//functions deal with memory allocation errors.
void *MyMalloc(size_t size)
{
	void *memptr;

	memptr = (void *) malloc(size);
	if (memptr == (void *) NULL) {
		fprintf(stderr,"\nError malloc:  Out of memory.\n");
		fprintf(stderr,"The model data is too big.\n");
		exit(1);
	}
	return(memptr);
}

void *MyRealloc(void *memblock, size_t size)
{
	void *oldbuffer;
	oldbuffer = memblock;
	if((memblock = realloc(memblock, size))==NULL)
	{
		free(oldbuffer);
		fprintf(stderr,"\nError realloc:  Out of memory.\n");
		fprintf(stderr,"The model data is too big.\n");
		exit(1);
	}
	return(memblock);
}

// Define a function to be called if new fails to allocate memory.
int MyNewHandler( size_t size ) // This function can be comment out on unix
{
	fprintf(stderr,"\nError new:  Out of memory.\n");
	fprintf(stderr,"The model data is too big.\n");
	exit(1);
}

int main(int argc, char* argv[])
{
    clock_t start, finish;
    double  duration;
    int filename_i=0;
    int filename_o=0;
	struct ESRIHeader eheader;
   
    //Initialisation;
    //_set_new_handler( MyNewHandler );//** This line should be commented out on unix
    m_bNeighbourCV = TRUE;
    m_fSigma = 0.4;
    m_nIterations = 20;
    m_nVIterations = 50;
    m_bAddVertices = FALSE;
	m_bZOnly = FALSE;

    /* parse command line */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'e':
                case 'E':
                    m_bNeighbourCV = FALSE;
                    break;
                case 't':
                case 'T':
                    i++;
                    sscanf(argv[i],"%f",&m_fSigma);
                    if ((m_fSigma<0)||(m_fSigma>1))
                    {
                        printf("Warning:\nThe threshold must be within (0,1)!\n");
                        printf("The default value [0.4] is used in the following computation!\n");
                        m_fSigma = 0.4;
                    }
                    break;
                case 'n':
                case 'N':
                    i++;
                    sscanf(argv[i],"%d",&m_nIterations);
                    if (m_nIterations<1)
                    {
                        printf("Warning:\nThe number of iteration for normal updating must be greater than 1!\n");
                        printf("The default value 20 is used in the following computation!\n");
                        m_nIterations = 20;
                    }
                    break;
                case 'v':
                case 'V':
                    i++;
                    sscanf(argv[i],"%d",&m_nVIterations);
                    if (m_nVIterations<1)
                    {
                        printf("Warning:\nThe number of iteration for vertex updating must be greater than 1!\n");
                        printf("The default value 50 is used in the following computation!\n");
                        m_nVIterations = 50;
                    }
                    break;
                case 'i':
                case 'I':
                    i++;
                    filename_i = i;
                    break;
                case 'o':
                case 'O':
                    i++;
                    filename_o = i;
                    break;
                case 'a':
                case 'A':
                    m_bAddVertices = TRUE;
                    break;
                case 'z':
                case 'Z':
                    m_bZOnly = TRUE;
                    break;
                default:
                    printf("unknown option %s\n",argv[i]);
                    options(argv[0]);
            }
        }
        else {
            printf("unknown option %s\n",argv[i]);
            options(argv[0]);
        }
    }

	
	/////////////////////////////////////////////////////////////////////////////////
//	filename_i = 2;
///////////////////////////////////////////////////////////////////////////////////
    if (filename_i == 0) {
        printf("Error: input filename required\n");
        options(argv[0]);
    }
	/////////////////////////////////////////////////////////////////////

    //Read file
    int fileext_i;
    int fileext_o;
    char filename[201];
    char pathname[206];
    char pathname_i[206];  // Only for copy prj file of .acs file
    char pathname_o[206];  // Only for copy prj file of .acs file
    int filelen = strlen(argv[filename_i]);
    strcpy(pathname,argv[filename_i]);
//    int filelen = 14;
//	strcpy(pathname,"m1_dem_utm.asc");
//	strcpy(pathname,"localities.xyz");
    pathname[filelen]='\0';
    fileext_i = FindInputExt(pathname);
    fileext_o = fileext_i;
    if (fileext_i==FILE_PLY2)
    {
        strncpy(filename,pathname,filelen-5);
        filename[filelen-5]='\0';
    }
    else
    {
        strncpy(filename,pathname,filelen-4);
        filename[filelen-4]='\0';
    }

    if (fileext_i==FILE_ESRI)
    {
		strcpy(pathname_i,filename);
		strcat(pathname_i,".prj");
    }

    printf("Input File: %s\n",pathname);
    FILE *fp = fopen(pathname, "rb");
    if (!fp) {
        printf("Can't open file to load!\n");
        return 0;
    }
    else
    {
        if (m_bNeighbourCV)
        {
            printf("Neighbourhood: Common Vertex\n");
        }
        else
        {
            printf("Neighbourhood: Common Edge\n");
        }
        printf("Threshold: %f\n",m_fSigma);
        printf("n1: %d\n",m_nIterations);
        printf("n2: %d\n",m_nVIterations);

        start = clock();
        printf("Read Model...");
        m_nNumFace = ReadData(fp, fileext_i,&eheader);
        finish = clock();
        duration = (double)(finish - start) / CLOCKS_PER_SEC;
        printf( "%10.3f seconds\n", duration );
    }
    fclose(fp);

    //Denoising Model...
    start = clock();
    printf("Denoising Model...");
    MeshDenoise(m_bNeighbourCV, m_fSigma, m_nIterations, m_nVIterations);
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "%10.3f seconds\n", duration );

    //Saving Model...
    start = clock();
    printf("Saving Model...");

    char szFileName[206];
    if (filename_o == 0)
    {
        strcpy(pathname,filename);   
        if (m_bNeighbourCV)
            strcat(pathname,"_V_");
        else
            strcat(pathname,"_E_");
        sprintf(szFileName,"%4.2f_",m_fSigma);
        strcat(pathname,szFileName);
        sprintf(szFileName,"%d_",m_nIterations);
        strcat(pathname,szFileName);
        sprintf(szFileName,"%d",m_nVIterations);
        strcat(pathname,szFileName);

        switch (fileext_i)
        {
/*        case FILE_GTS:
            strcat(pathname,".gts");
            break;

*/        case FILE_OBJ:
            strcat(pathname,".obj");
            break;

        case FILE_OFF:
            strcat(pathname,".off");
            break;

        case FILE_PLY:
            strcat(pathname,".ply");
            break;

        case FILE_PLY2:
            strcat(pathname,".ply2");
            break;

/*        case FILE_SMF:
            strcat(pathname,".smf");
            break;

        case FILE_STL:
            strcat(pathname,".stl");
            break;

        case FILE_WRL:
            strcat(pathname,".wrl");
            break;
*/
        case FILE_XYZ:
            strcat(pathname,".xyz");
            break;

		case FILE_ESRI:
			strcpy(pathname_o,pathname);
			strcat(pathname_o,".prj");
			strcat(pathname,".asc");
            break;

		default:
            fileext_o = FILE_OFF;
            strcat(pathname,".off");
        }
    }
    else
    {   
        strcpy(szFileName,argv[filename_o]);
        filelen = strlen(argv[filename_o]);
        szFileName[filelen]='\0';
        fileext_o = FindOutputExt(szFileName);
        if (fileext_o==FILE_DFLT)
        {
            fileext_o = fileext_i;
            switch (fileext_i)
            {
    /*        case FILE_GTS:
                strcat(szFileName,".gts");
                break;

*/            case FILE_OBJ:
                strcat(szFileName,".obj");
                break;

            case FILE_OFF:
                strcat(szFileName,".off");
                break;

            case FILE_PLY:
                strcat(szFileName,".ply");
                break;
   
            case FILE_PLY2:
                strcat(szFileName,".ply2");
                break;

    /*        case FILE_SMF:
                strcat(szFileName,".smf");
                break;

            case FILE_STL:
                strcat(szFileName,".stl");
                break;

            case FILE_WRL:
                strcat(szFileName,".wrl");
                break;
    */
            case FILE_XYZ:
                strcat(szFileName,".xyz");
                break;

			case FILE_ESRI:
				strcpy(pathname_o,szFileName);
				strcat(pathname_o,".prj");
	            strcat(szFileName,".asc");
		        break;

			default:
				fileext_o = FILE_OFF;
				strcat(szFileName,".off");
			}
		} else if (fileext_o ==FILE_ESRI){
			strcpy(pathname_o,szFileName);
			pathname_o[filelen-4]='\0';
			strcat(pathname_o,".prj");
		}

		if (!strcicmp(szFileName,pathname))
		{
            printf("\nWarning: The input and output file names are the same.\n");
            printf("Output file names are renamed with 'ERR' as the prefix.\n");
            strcpy(szFileName, "ERR");
			if (fileext_o ==FILE_ESRI){
				strcat(szFileName,pathname_o);
				strcpy(pathname_o, szFileName);
			}
			strcpy(szFileName, "ERR");
			strcat(szFileName,pathname);
        }
        strcpy(pathname, szFileName);
    }

    fp = fopen(pathname, "w");
    if (!fp) {
        printf("Can't open file to write!\n");
        return 0;
    }


    SaveData(fp,fileext_o,&eheader);
    fclose(fp);

	FILE *in,*out;
	char ch;
	if (fileext_o==FILE_ESRI)
	{
		if((in=fopen(pathname_i,"rb"))==NULL)
			printf("No .prj file is found.\n");
		else if((out=fopen(pathname_o,"wb"))==NULL)
			printf("Cannot open the target .prj file.\n");
		else{

			/* start copy */
			ch = getc(in);
			while(!feof(in)){
				putc(ch,out);
				ch = getc(in);
			}
			fclose(in);
			fclose(out);
		}
	}

    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "%10.3f seconds\n", duration );
    return 0;
}

int strcicmp(const char *string1, const char *string2)
{
    int result, len1, len2;
    len1 = strlen(string1);
    len2 = strlen(string2);
    len1 = (len1 <len2) ? len1 : len2;
    for(int i =0;i<len1+1;i++)
    {
        result = string1[i]-string2[i];
        if((string1[i]>='A')&&(string1[i]<='Z'))
            result += 'a'-'A';
        if((string2[i]>='A')&&(string2[i]<='Z'))
            result -= 'a'-'A';
        if (result!=0)
            break;
    }
    return result;
}


int FindInputExt(char* pPath)
{
    int nfile_ext = 0;
    char pFile[201];
    char fileext[6];
    int filelen = strlen(pPath);
    int endpoint = filelen;
    char *pdest = strrchr(pPath,'.');

    if (pdest!=NULL)
    {
        endpoint = (int )(pdest-pPath);
        if ((filelen-endpoint)>5)
        {
            strcpy(pFile,pPath);
            fileext[0] ='\0';
        }
        else
        {
            endpoint=(endpoint<200 ? endpoint : 200);
            strncpy(pFile,pPath,endpoint);
            pFile[endpoint]='\0';
            strcpy(fileext,pdest);
        }
    }
    else
    {
        strcpy(pFile,pPath);
        fileext[0] ='\0';
    }

    if(!strcicmp(fileext,".gts"))
        nfile_ext = FILE_GTS;
    else if(!strcicmp(fileext,".obj"))
        nfile_ext = FILE_OBJ;
    else if(!strcicmp(fileext,".off"))
        nfile_ext = FILE_OFF;
    else if(!strcicmp(fileext,".ply"))
        nfile_ext = FILE_PLY;
    else if(!strcicmp(fileext,".ply2"))
        nfile_ext = FILE_PLY2;
    else if(!strcicmp(fileext,".smf"))
        nfile_ext = FILE_SMF;
    else if(!strcicmp(fileext,".stl"))
        nfile_ext = FILE_STL;
    else if(!strcicmp(fileext,".wrl"))
        nfile_ext = FILE_WRL;
    else if(!strcicmp(fileext,".xyz"))
        nfile_ext = FILE_XYZ;
    else if(!strcicmp(fileext,".asc"))
	{
		m_bZOnly = TRUE;
        nfile_ext = FILE_ESRI;
	}
    else if(fileext[0]=='\0')
    {
        nfile_ext = FILE_OFF;
        strcat(pPath,".off");
    }
    else{
        printf("This input file format is not supported!\n");
        exit(-1);
    }
    return nfile_ext;
}

int FindOutputExt(char* pPath)
{
    int nfile_ext = 0;
    char pFile[201];
    char fileext[6];
    int filelen = strlen(pPath);
    int endpoint = filelen;
    char *pdest = strrchr(pPath,'.');

    if (pdest!=NULL)
    {
        endpoint = (int )(pdest-pPath);
        if ((filelen-endpoint)>5)
        {
            strcpy(pFile,pPath);
            fileext[0] ='\0';
        }
        else
        {
            endpoint=(endpoint<200 ? endpoint : 200);
            strncpy(pFile,pPath,endpoint);
            pFile[endpoint]='\0';
            strcpy(fileext,pdest);
        }
    }
    else
    {
        strcpy(pFile,pPath);
        fileext[0] ='\0';
    }

/*    if(!strcicmp(fileext,".gts"))
        nfile_ext = FILE_GTS;
    else*/ if(!strcicmp(fileext,".obj"))
        nfile_ext = FILE_OBJ;
    else if(!strcicmp(fileext,".off"))
        nfile_ext = FILE_OFF;
    else if(!strcicmp(fileext,".ply"))
        nfile_ext = FILE_PLY;
    else if(!strcicmp(fileext,".ply2"))
        nfile_ext = FILE_PLY2;
/*    else if(!strcicmp(fileext,".smf"))
        nfile_ext = FILE_SMF;
    else if(!strcicmp(fileext,".stl"))
        nfile_ext = FILE_STL;
    else if(!strcicmp(fileext,".wrl"))
        nfile_ext = FILE_WRL;
*/    else if(!strcicmp(fileext,".xyz"))
        nfile_ext = FILE_XYZ;
    else if(!strcicmp(fileext,".asc"))
        nfile_ext = FILE_ESRI;

    else if(fileext[0]=='\0')
    {
        nfile_ext = FILE_DFLT;
        //strcat(pPath,".off");
    }
    else{
        printf("\nWarning:\nThis output file format is not supported!\n");
        printf("Default file format (.off) is used!\n");
        nfile_ext = FILE_OFF;
        strcat(pPath,".off");
    }
    return nfile_ext;
}

int ReadData(FILE * fp, int nfileext, struct ESRIHeader* header)
{
    m_nNumFace=0;

    switch (nfileext)
    {
    case FILE_GTS:
        ReadGTS(fp);
        break;

    case FILE_OBJ:
        ReadOBJ(fp);
        break;

    case FILE_OFF:
        ReadOFF(fp);
        break;

    case FILE_PLY:
        ReadPLY(fp);
        break;

    case FILE_PLY2:
        ReadPLY2(fp);
        break;

    case FILE_SMF:
        ReadSMF(fp);
        break;

    case FILE_STL:
        ReadSTL(fp);
        break;

    case FILE_WRL:
        ReadWRL(fp);
        break;

    case FILE_XYZ:
        ReadXYZ(fp);
        break;

    case FILE_ESRI:
        ReadESRI(fp,header);
        break;

	default:
        return 0;
    }

    ScalingBox(); // scale to a box
    ComputeNormal(FALSE);

    m_nNumVertexP = m_nNumVertex;
    m_nNumFaceP = m_nNumFace;
    m_pf3VertexP = new FVECTOR3[m_nNumVertexP];
    m_pn3FaceP = new NVECTOR3[m_nNumFaceP];
    m_pf3VertexNormalP = new FVECTOR3[m_nNumVertexP];
    m_pf3FaceNormalP = new FVECTOR3[m_nNumFaceP];

    for (int i=0;i<m_nNumVertex;i++)
    {
        VEC3_ASN_OP(m_pf3VertexP[i],=,m_pf3Vertex[i]);
        VEC3_ASN_OP(m_pf3VertexNormalP[i],=,m_pf3VertexNormal[i]);
    }
    for (int i=0;i<m_nNumFace;i++)
    {
        VEC3_ASN_OP(m_pn3FaceP[i],=,m_pn3Face[i]);
        VEC3_ASN_OP(m_pf3FaceNormalP[i],=,m_pf3FaceNormal[i]);
    }

    return m_nNumFace;
}

void ReadGTS(FILE* fp)
{
    int i;
    int tmp, tmp1, tmp2, tmp3;
    char sTmp[200],sTmp1[200];
    NVECTOR2 *edge;

    do
    {
        fgets(sTmp, 200, fp);
        sscanf(sTmp, "%s", sTmp1);
    }while (sTmp1[0]=='#');


    i = sscanf(sTmp,"%d%d%d", &m_nNumVertex, &tmp, &m_nNumFace);
    while(i!=3)
    {
        i = fscanf(fp,"%d%d%d", &m_nNumVertex, &tmp, &m_nNumFace);
    }

    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    edge = new NVECTOR2[tmp];
    m_pn3Face = new NVECTOR3[m_nNumFace];

    for (i=0;i<m_nNumVertex;i++)
    {
        fgets(sTmp, 200, fp);
        sscanf(sTmp,"%f%f%f", &(m_pf3Vertex[i][0]), &(m_pf3Vertex[i][1]), &(m_pf3Vertex[i][2]));
    }
    for (i=0;i<tmp;i++)
    {
        fscanf(fp,"%d%d", &(edge[i][0]), &(edge[i][1]));
    }
    for (i=0;i<m_nNumFace;i++)
    {
        fscanf(fp,"%d%d%d", &tmp1, &tmp2, &tmp3);
        m_pn3Face[i][0] = edge[tmp1-1][0]-1;
        m_pn3Face[i][1] = edge[tmp1-1][1]-1;
        if ((edge[tmp2-1][0] == edge[tmp1-1][0])||(edge[tmp2-1][0] == edge[tmp1-1][1]))
            m_pn3Face[i][2] = edge[tmp2-1][1]-1;
        else
            m_pn3Face[i][2] = edge[tmp2-1][0]-1;
    }
    delete []edge;
}

void ReadOBJ(FILE* fp)
{
    int i, j;
    int nTmp,nTmp1;
    char sTmp[200], sTmp1[200];
    FVECTOR3 *vVertex;
    NVECTOR3 * tTriangle;

    vVertex = (FVECTOR3 *)MyMalloc(10000* sizeof(FVECTOR3));
    tTriangle = (NVECTOR3 *)MyMalloc(10002* sizeof(NVECTOR3));

    m_nNumVertex = m_nNumFace = 0;
    while (!feof(fp))
    {
        fgets(sTmp, 200, fp);
        if(sTmp[0]=='v')
        {
            if((sTmp[1]=='t')||(sTmp[1]=='n'))
            {
                printf("This OBJ file is not supported!\n");
                m_nNumVertex = m_nNumFace = 0;
                free(vVertex);
                free(tTriangle);
                return;
            }
            else
            {
                sscanf(sTmp,"%s%f%f%f", sTmp1, &(vVertex[m_nNumVertex][0]), \
                    &(vVertex[m_nNumVertex][1]),&(vVertex[m_nNumVertex][2]));
                m_nNumVertex++;
                if (!(m_nNumVertex % 10000))
                    vVertex = (FVECTOR3 *)MyRealloc(vVertex, (m_nNumVertex+10000)* sizeof(FVECTOR3));
            }
        }
        else if(sTmp[0]=='f')
        {
            j = sscanf(sTmp,"%s%d%d%d%d%d", sTmp1, &(tTriangle[m_nNumFace][0]), \
                &(tTriangle[m_nNumFace][1]),&(tTriangle[m_nNumFace][2]), &nTmp, &nTmp1);
            if (j==4)
            {
                tTriangle[m_nNumFace][0]--;
                tTriangle[m_nNumFace][1]--;
                tTriangle[m_nNumFace][2]--;
            }
            else if(j==5)
            {
                tTriangle[m_nNumFace][0]--;
                tTriangle[m_nNumFace][1]--;
                tTriangle[m_nNumFace][2]--;
                m_nNumFace++;
                if (!(m_nNumFace % 10000))
                    tTriangle = (NVECTOR3 *)MyRealloc(tTriangle, (m_nNumFace+10002)* sizeof(NVECTOR3));
                tTriangle[m_nNumFace][0] = tTriangle[m_nNumFace-1][2];
                tTriangle[m_nNumFace][1] = (--nTmp);
                tTriangle[m_nNumFace][2] = tTriangle[m_nNumFace-1][0];
            }
            else
            {
                printf("This OBJ file is not supported!\n");
                m_nNumVertex = m_nNumFace = 0;
                free(vVertex);
                free(tTriangle);
                return;
            }
            m_nNumFace++;
            if (!(m_nNumFace % 10000))
                tTriangle = (NVECTOR3 *)MyRealloc(tTriangle, (m_nNumFace+10002)* sizeof(NVECTOR3));
        }
    }

    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    for(i=0; i<m_nNumVertex; i++)
    {
        VEC3_ASN_OP(m_pf3Vertex[i], =, vVertex[i]);
    }
    free(vVertex);

     m_pn3Face = new NVECTOR3[m_nNumFace];
    for(i=0; i<m_nNumFace; i++)
    {
        VEC3_ASN_OP(m_pn3Face[i], =, tTriangle[i]);
    }
    free(tTriangle);
}

void ReadOFF(FILE* fp)
{
    int i,j;
    char tmp[300];

    fscanf(fp,"%s", tmp);
    if ((strncmp(tmp, "OFF",3))||(fscanf(fp,"%d %d %d",&m_nNumVertex,&m_nNumFace, &j)<2))
    {
        printf("The file is not a valid OFF one!111\n");
        m_nNumVertex = m_nNumFace = 0;
        return;
    }

    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    m_pn3Face = new NVECTOR3[m_nNumFace];

    for (i=0;i<m_nNumVertex;i++)
    {
        fscanf(fp,"%f%f%f,", &(m_pf3Vertex[i][0]), &(m_pf3Vertex[i][1]), &(m_pf3Vertex[i][2]));       
    }
    for (i=0;i<m_nNumFace;i++)
    {
        fscanf(fp,"%d%d%d%d", &j, &(m_pn3Face[i][0]), &(m_pn3Face[i][1]), &(m_pn3Face[i][2]));       
    }
}

void ReadPLY(FILE* fp)
{
    int i,j;
    char seps[]  = " ,\t\n\r";
    char sTmp[200], *sTmp1;
    int plyType = PLY_ASCII;
    int nVFreeByte = 0;

    fgets(sTmp, 200, fp);
    sTmp[3] ='\0';

    if (strcicmp(sTmp, "ply"))
    {
        printf("This file is not a valid PLY one!");
        m_nNumVertex = m_nNumFace = 0;
        return;
    }


    //read the file header
    while (!feof(fp))
    {
        fgets(sTmp, 200, fp);
        sTmp1 = strtok(sTmp, seps);
        if (!strcicmp(sTmp1, "format"))
        {
        //    sscanf(sTmp+6 , "%s", sTmp1);       
            sTmp1 = strtok( NULL, seps );
            if (!strcicmp(sTmp1, "ascii"))
                plyType = PLY_ASCII;
            else if (!strcicmp(sTmp1, "binary_little_endian"))
                plyType = PLY_BLITTLE;
            else if (!strcicmp(sTmp1, "binary_big_endian"))
                plyType = PLY_BBIG;
            else
            {
                printf("This format is not supported.!\n");
                return;
            }
            sTmp1 = strtok(NULL,seps);
            if (strcicmp(sTmp1, "1.0"))
            {
                printf("Only PLY format version 1.0 is supported!");
                return;
            }
            break;
        }
    }

    m_nNumVertex = 0;
    m_nNumFace = 0;
    bool bVertex = FALSE;
    bool bFace = FALSE;
    while (!feof(fp))
    {
        fgets(sTmp,200,fp);
        sTmp1 = strtok(sTmp, seps);
        if (!strcicmp(sTmp, "element"))
        {
            sTmp1 = strtok(NULL,seps);
            if (!strcicmp(sTmp1, "vertex"))
            {
                sTmp1 = strtok(NULL,seps);
                m_nNumVertex = atoi(sTmp1);
                bVertex = TRUE;
                bFace = FALSE;
            }
            else if (!strcicmp(sTmp1, "face"))
            {
                sTmp1 = strtok(NULL,seps);
                m_nNumFace = atoi(sTmp1);
                bVertex = FALSE;
                bFace = TRUE;
            }
            else
            {
                bVertex = FALSE;
                bFace = FALSE;
            }
        }
        else if (!strcicmp(sTmp, "property"))
        {
            if(bVertex)
            {
                sTmp1 = strtok(NULL,seps);
                if (!(strcicmp(sTmp1, "float")&&strcicmp(sTmp1, "float32")))
                {
                    sTmp1 = strtok(NULL,seps);
                    if (strcicmp(sTmp1, "x")&& strcicmp(sTmp1, "y")&& strcicmp(sTmp1, "z"))
                        nVFreeByte += 4;
                }
                else if (!strcicmp(sTmp1, "char"))
                        nVFreeByte += 1;
                else if (!strcicmp(sTmp1, "uchar"))
                        nVFreeByte += 1;
                else if (!strcicmp(sTmp1, "uint8"))
                        nVFreeByte += 1;
                else if (!strcicmp(sTmp1, "short"))
                        nVFreeByte += 2;
                else if (!strcicmp(sTmp1, "ushort"))
                        nVFreeByte += 2;
                else if (!strcicmp(sTmp1, "int"))
                        nVFreeByte += 4;
                else if (!strcicmp(sTmp1, "uint"))
                        nVFreeByte += 4;
                else if (!strcicmp(sTmp1, "double"))
                        nVFreeByte += 8;
            }
            else if(bFace)
            {
                //to be further developped.
            }
        }
        else if (!strcicmp(sTmp1, "end_header"))
            break;
    }

    if (plyType==PLY_ASCII)
    {
        m_pf3Vertex = new FVECTOR3[m_nNumVertex];
        m_pn3Face = new NVECTOR3[m_nNumFace];
   
        for (i=0;i<m_nNumVertex;i++)
        {
            fgets(sTmp,200,fp);
            j = sscanf(sTmp,"%f%f%f", &(m_pf3Vertex[i][0]), &(m_pf3Vertex[i][1]), &(m_pf3Vertex[i][2]));
                    if (j!=3)
                        break;
        }
        for (i=0;i<m_nNumFace;i++)
        {
            fgets(sTmp,200,fp);   
            sscanf(sTmp,"%d%d%d%d,", &j, &(m_pn3Face[i][0]), &(m_pn3Face[i][1]), &(m_pn3Face[i][2]));
        }
    }
    else // PLY_BBIG and PLYBLITTLE
    {
        m_pf3Vertex = new FVECTOR3[m_nNumVertex];
        m_pn3Face = new NVECTOR3[m_nNumFace];

        for (i=0; i<m_nNumVertex; i++)
        {
            fread(&(m_pf3Vertex[i]),sizeof(float),3,fp);
            if (plyType==PLY_BBIG)
            {
                char* c = (char*) &(m_pf3Vertex[i][0]);
                char cTmp;
               
                cTmp = c[0]; c[0] = c[3]; c[3] = cTmp;
                cTmp = c[1]; c[1] = c[2]; c[2] = cTmp;
                float *x = (float *) &c[0];
                m_pf3Vertex[i][0] = *x;

                c = (char*) &(m_pf3Vertex[i][1]);
                cTmp = c[0]; c[0] = c[3]; c[3] = cTmp;
                cTmp = c[1]; c[1] = c[2]; c[2] = cTmp;
                x = (float *) &c[0];
                m_pf3Vertex[i][1] = *x;

                c = (char*) &(m_pf3Vertex[i][2]);
                cTmp = c[0]; c[0] = c[3]; c[3] = cTmp;
                cTmp = c[1]; c[1] = c[2]; c[2] = cTmp;
                x = (float *) &c[0];
                m_pf3Vertex[i][2] = *x;
            }
            // special///////////////////////////////////
            if (nVFreeByte != 0)
            {
                char cTmp[200];
                fread(cTmp,sizeof(char),nVFreeByte,fp);
            }
        }
        for (i=0; i<m_nNumFace; i++)
        {
            unsigned char cTmp;
            fread(&cTmp,sizeof(unsigned char),1,fp);
            fread(&(m_pn3Face[i]),sizeof(int),3,fp);
            if (plyType==PLY_BBIG)
            {
                char* c = (char*) &(m_pn3Face[i][0]);
                char cTmp;
                cTmp = c[0]; c[0] = c[3]; c[3] = cTmp;
                cTmp = c[1]; c[1] = c[2]; c[2] = cTmp;

                c = (char*) &(m_pn3Face[i][1]);
                cTmp = c[0]; c[0] = c[3]; c[3] = cTmp;
                cTmp = c[1]; c[1] = c[2]; c[2] = cTmp;

                c = (char*) &(m_pn3Face[i][2]);
                cTmp = c[0]; c[0] = c[3]; c[3] = cTmp;
                cTmp = c[1]; c[1] = c[2]; c[2] = cTmp;

            }
        }
        return;
    }
}

void ReadPLY2(FILE* fp)
{
    int i, j;

    if(fscanf(fp,"%d%d",&m_nNumVertex,&m_nNumFace)!=2)
    {
        printf("This is not a valid PLY2 file!\n");
        m_nNumVertex = m_nNumFace = 0;
        return;
    }

    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    m_pn3Face = new NVECTOR3[m_nNumFace];

    for (i=0;i<m_nNumVertex;i++)
    {
        fscanf(fp,"%f%f%f", &(m_pf3Vertex[i][0]), &(m_pf3Vertex[i][1]), &(m_pf3Vertex[i][2]));
    }
    for (i=0;i<m_nNumFace;i++)
    {
        fscanf(fp,"%d%d%d%d", &j, &(m_pn3Face[i][0]), &(m_pn3Face[i][1]), &(m_pn3Face[i][2]));
    }
}

void ReadSMF(FILE* fp)
{
    int i;
    char sTmp[200], sTmp1[200];

    FVECTOR3 *vVertex;
    NVECTOR3 * tTriangle;

    vVertex = (FVECTOR3 *)MyMalloc(10000* sizeof(FVECTOR3));
    tTriangle = (NVECTOR3 *)MyMalloc(10000* sizeof(NVECTOR3));

    m_nNumVertex = m_nNumFace = 0;
    while (!feof(fp))
    {
        fgets(sTmp, 200, fp);
        if(sTmp[0]=='v')
        {
            sscanf(sTmp,"%s%f%f%f", sTmp1, &(vVertex[m_nNumVertex][0]), \
                &(vVertex[m_nNumVertex][1]),&(vVertex[m_nNumVertex][2]));
            m_nNumVertex +=1;
            if (!(m_nNumVertex % 10000))
                vVertex = (FVECTOR3 *)MyRealloc(vVertex, (m_nNumVertex+10000)* sizeof(FVECTOR3));
        }
        else if((sTmp[0]=='t')||(sTmp[0]=='f'))
        {
            sscanf(sTmp,"%s%d%d%d", sTmp1, &(tTriangle[m_nNumFace][0]), \
                &(tTriangle[m_nNumFace][1]),&(tTriangle[m_nNumFace][2]));
            tTriangle[m_nNumFace][0]--;
            tTriangle[m_nNumFace][1]--;
            tTriangle[m_nNumFace][2]--;
            m_nNumFace +=1;
            if (!(m_nNumFace % 10000))
                tTriangle = (NVECTOR3 *)MyRealloc(tTriangle, (m_nNumFace+10000)* sizeof(NVECTOR3));
        }
    }

    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    for(i=0; i<m_nNumVertex; i++)
    {
        VEC3_ASN_OP(m_pf3Vertex[i], =, vVertex[i]);
    }
    free(vVertex);

     m_pn3Face = new NVECTOR3[m_nNumFace];
    for(i=0; i<m_nNumFace; i++)
    {
        VEC3_ASN_OP(m_pn3Face[i], =, tTriangle[i]);
    }
    free(tTriangle);
}

void ReadSTL(FILE* fp)
{
    int i, j, k, m;
    int nTmp, nTmp1, nTmp2;
    char sTmp[200], sTmp1[40], sTmp2[40];
    FVECTOR3 *vVertex;
    NVECTOR3 *tTriangle;
    int *indexVertex;
    int **iiVertex;
    bool *biVertex;

    m_nNumVertex = m_nNumFace = 0;
    vVertex = (FVECTOR3 *)MyMalloc(10002* sizeof(FVECTOR3));
    while(!feof(fp))  //read the vertex data
    {
        fgets(sTmp, 200, fp);
        sscanf(sTmp,"%s%s", sTmp1, sTmp2);
        if(!(strcicmp(sTmp1, "outer")) && !(strcicmp(sTmp2,"loop")))
        {
            for(i=0; i<3; i++)
            {
                fgets(sTmp, 200, fp);
                sscanf(sTmp,"%s%f%f%f", sTmp1, &vVertex[m_nNumVertex][0],\
                    &vVertex[m_nNumVertex][1], &vVertex[m_nNumVertex][2]);
                if(strcicmp(sTmp1,"vertex"))
                {
                    printf("This STL file is not supported!\n");
                    m_nNumVertex = m_nNumFace = 0;
                    free(vVertex);
                    return;
                }
                else
                {
                    m_nNumVertex++;
                    if (!(m_nNumVertex % 10000))
                    vVertex = (FVECTOR3 *)MyRealloc(vVertex, (m_nNumVertex+10002)* sizeof(FVECTOR3));
                }
            }
            fgets(sTmp, 200, fp);
            sscanf(sTmp,"%s", sTmp1);
            if(strcicmp(sTmp1,"endloop"))
            {
                printf("This STL file is not supported!\n");
                m_nNumVertex = m_nNumFace = 0;
                free(vVertex);
                return;
            }
        }   
    }

    indexVertex = (int *)MyMalloc(m_nNumVertex*sizeof(int));
    biVertex = (bool *)MyMalloc(m_nNumVertex*sizeof(bool));
    for(i=0; i<m_nNumVertex; i++)
    {
        indexVertex[i] = i;
        biVertex[i]=TRUE;
    }

    m_nNumFace = m_nNumVertex/3;
    tTriangle = (NVECTOR3 *)MyMalloc(m_nNumFace* sizeof(NVECTOR3));
    iiVertex =(int **) MyMalloc(m_nNumVertex * sizeof(int *));

    nTmp = 0; // nTmp is now the number of deleted vertices
    for(i=0; i<m_nNumFace; i++) //get rid of the redundant vertex data
    {
        for(m=0; m<3; m++)
        {
            if(biVertex[3*i+m]) //the vertex 3*i+m has no identical one
            {
                iiVertex[3*i+m] = (int *)MyMalloc(6*sizeof(int));
                nTmp1 = 0;
                for(j=i+1; j<m_nNumFace; j++)
                {
                    for(k=0; k<3; k++)
                    {
                        if(biVertex[3*j+k]) //the vertex 3*j+k has no identical one
                        {
                            if(VEC3_EQ(vVertex[3*j+k], vVertex[3*i+m]))//vertex 3*j+k is identical to 3*i+m
                            {
                                indexVertex[3*j+k] = indexVertex[3*i+m];
                                biVertex[3*j+k] = FALSE;
                                iiVertex[3*i+m][++nTmp1] =3*j+k;
                                iiVertex[3*i+m][0] = nTmp1;
                                nTmp++;
                                if(!(nTmp1%5))
                                    iiVertex[3*i+m] = (int *)MyRealloc(iiVertex[3*i+m], (nTmp1+6)*sizeof(int));
                                break;
                            }
                        }
                    }
                }
            }
        }
        tTriangle[i][0]=indexVertex[3*i];
        tTriangle[i][1]=indexVertex[3*i+1];
        tTriangle[i][2]=indexVertex[3*i+2];
    }

    j=m_nNumVertex-1;
    for(i=0; i<m_nNumVertex-nTmp; i++)
    {
        if(!biVertex[i])
        {
            while(!biVertex[j])
            {
                j--;
            }
            indexVertex[i] = j;
            indexVertex[j] = i;   
            for(k=1; k<iiVertex[j][0]+1; k++)
            {
                indexVertex[iiVertex[j][k]] = i;
            }
            j--;
        }
    }
   
    for(i=m_nNumVertex-nTmp; i<m_nNumVertex;i++)
    {
            nTmp1 = i/3;
            nTmp2 = i%3;
            tTriangle[nTmp1][nTmp2] = indexVertex[i];
    }

    m_nNumVertex = m_nNumVertex-nTmp;
    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    for(i=0; i<m_nNumVertex; i++)
        VEC3_ASN_OP(m_pf3Vertex[i], =, vVertex[indexVertex[i]]);

    m_pn3Face = new NVECTOR3[m_nNumFace];
    for(i=0; i<m_nNumFace; i++)
        VEC3_ASN_OP(m_pn3Face[i], =, tTriangle[i]);

    for(i=0; i<m_nNumVertex+nTmp; i++)
    {
        if(biVertex[i])
            free(iiVertex[i]);
    }
    free(iiVertex);
    free(indexVertex);
    free(biVertex);
    free(vVertex);
    free(tTriangle);
}

void ReadWRL(FILE* fp)
{
    int i,j;
    char sTmp[200], *tmp, *sTmp1;
    char seps[]  = " ,\t\n";
    FVECTOR3    tmpV;
    NVECTOR3    tmpT;
    FVECTOR3*    fVertex ;
    NVECTOR3*   nTriangle;

    m_nNumVertex = 0;
//    char* key = "point [";
    fVertex = (FVECTOR3*) MyMalloc(1000*sizeof(FVECTOR3));
    tmp = new char[100];
    fscanf(fp,"%s", tmp);
    //while (tmp!="")
    while (*tmp != '\0')
    {
        fscanf(fp,"%s", tmp);
        if(!strcicmp(tmp,"point"))
        {
            fscanf(fp,"%s", tmp);
            if(!strcicmp(tmp,"["))
            {
                while (TRUE)
                {
                    j = fscanf(fp,"%f%f%f,", &tmpV[0], &tmpV[1], &tmpV[2]);
                    if (j!=3)
                        break;
                    VEC3_ASN_OP(fVertex[m_nNumVertex],=, tmpV);
                    m_nNumVertex +=1;
                    if(!(m_nNumVertex %1000))
                        fVertex = (FVECTOR3* )MyRealloc(fVertex, (m_nNumVertex+1000)*sizeof(FVECTOR3));
                }
                break;
            }
        }
    }
    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    for (i =0; i<m_nNumVertex; i++)
        VEC3_ASN_OP(m_pf3Vertex[i], =, fVertex[i]);
    free(fVertex);

    m_nNumFace = 0;
//    key = "coordIndex [";
    nTriangle = (NVECTOR3*) MyMalloc(1000*sizeof(NVECTOR3));
    fscanf(fp,"%s", tmp);
    //while (tmp!="")
    while (*tmp != '\0')
    {
        if(!strcicmp(tmp,"coordIndex"))
        {
            fscanf(fp,"%s", tmp);
            if(!strcicmp(tmp,"["))
            {
                fgets(sTmp, 200, fp);
                while (TRUE)
                {
                    fgets(sTmp, 200, fp);
                    sTmp1 = strtok(sTmp, seps);
                    for(i=0; i<3;i++)
                    {
                        if(sTmp1!=NULL)
                        {
                            sscanf(sTmp1, "%d",&tmpT[i]);
                        }
                        else
                            break;
                        sTmp1 = strtok( NULL, seps );
                    }
                    //sTmp1 = strtok(sTmp, seps);
                    if(sTmp1!=NULL)
                    {
                        i=sscanf(sTmp1,"%d,", &j);
                    }
                    else
                        break;
                    if ((i == 0) ||(j!=-1))
                        break;
                    VEC3_ASN_OP(nTriangle[m_nNumFace], =, tmpT);
                    m_nNumFace +=1;
                    if(!(m_nNumFace %1000))
                        nTriangle = (NVECTOR3* )MyRealloc(nTriangle, (m_nNumFace+1000)*sizeof(NVECTOR3));
                }
                break;
            }
        }
        else
            fscanf(fp,"%s", tmp);
    }
    m_pn3Face = new NVECTOR3[m_nNumFace];
    for (i =0; i<m_nNumFace; i++)
        VEC3_ASN_OP(m_pn3Face[i], =, nTriangle[i]);
    free(nTriangle);
    delete tmp;
}

//extern void triangulate(char *, struct triangulateio *,
//                struct triangulateio *, struct triangulateio *);
void ReadXYZ(FILE* fp)
{
    int i,nTmp;
    float fTmp0,fTmp1,fTmp2;
    struct triangulateio in, out, vorout;
    char sTmp[200],sTmp1[200];
    FVECTOR3 *vVertex;

    vVertex = (FVECTOR3 *)MyMalloc(10000* sizeof(FVECTOR3));
    m_nNumVertex = 0;
	while (!feof(fp)){
		sTmp[0]=sTmp1[0]='\0';
		fgets(sTmp, 200, fp);
		sscanf(sTmp, "%s", sTmp1);
		if(!(sTmp1[0]<'0')||(sTmp1[0]>'9')){
			sscanf(sTmp,"%f %f %f", &(vVertex[m_nNumVertex][0]), \
				&(vVertex[m_nNumVertex][1]),&(vVertex[m_nNumVertex][2]));
			m_nNumVertex++;
			if (!(m_nNumVertex % 10000))
				vVertex = (FVECTOR3 *)MyRealloc(vVertex, (m_nNumVertex+10000)* sizeof(FVECTOR3));
		}
	}

    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    for(i=0; i<m_nNumVertex; i++)
    {
        VEC3_ASN_OP(m_pf3Vertex[i], =, vVertex[i]);
    }
    free(vVertex);

    in.numberofpoints=m_nNumVertex;
    in.numberofpointattributes = 1;
    in.pointlist = (REAL *) MyMalloc(in.numberofpoints * 2 * sizeof(REAL));
    in.pointattributelist = (REAL *) MyMalloc(in.numberofpoints * in.numberofpointattributes * sizeof(REAL));
    for(i=0;i<m_nNumVertex;i++)
    {
        fscanf(fp, "%f%f%f",&fTmp0,&fTmp1,&fTmp2);
        in.pointlist[i*2]=m_pf3Vertex[i][0];
        in.pointlist[i*2+1]=m_pf3Vertex[i][1];
        in.pointattributelist[i]=m_pf3Vertex[i][2];
    }
    in.pointmarkerlist=(int *) NULL;

    out.pointlist = (REAL *) NULL;
    out.pointattributelist = (REAL *) NULL;
    out.trianglelist = (int *) NULL;

    printf("\nTriangulation...\n");
    if(m_bAddVertices)
        triangulate("zqBQ",&in,&out,&vorout);
    else
        triangulate("zBQ",&in,&out,&vorout);

    m_nNumVertex = out.numberofpoints;
    m_pf3Vertex = new FVECTOR3[m_nNumVertex];
    for (i=0; i<m_nNumVertex; i++)
    {
        m_pf3Vertex[i][0]=out.pointlist[2*i];
        m_pf3Vertex[i][1]=out.pointlist[2*i+1];
        m_pf3Vertex[i][2]=out.pointattributelist[i];
    }

    m_nNumFace = out.numberoftriangles;
    m_pn3Face = new NVECTOR3[m_nNumFace];
    for (i=0; i<m_nNumFace; i++)
    {
        m_pn3Face[i][0] = out.trianglelist[3*i];
        m_pn3Face[i][1] = out.trianglelist[3*i+1];
        m_pn3Face[i][2] = out.trianglelist[3*i+2];
    }
    free(in.pointlist);
    free(in.pointattributelist);
    free(in.pointmarkerlist);
    free(out.pointlist);
    free(out.pointattributelist);
    free(out.trianglelist);
}

void ReadESRI(FILE* fp, struct ESRIHeader* header)
{
    int i,ii,j,k,kk[4],nTmp,nTotal;
    float fTmp0,fTmp1,fTmp2;
	char sTmp[40];
	double * value, fTmp;

	fscanf(fp,"%s %d", sTmp, &(header->ncols));
	fscanf(fp,"%s %d", sTmp, &(header->nrows));
	fscanf(fp,"%s %lf", sTmp, &(header->xllcorner));
	fscanf(fp,"%s %lf", sTmp, &(header->yllcorner));
	fscanf(fp,"%s %lf", sTmp, &(header->cellsize));
	fscanf(fp,"%s %lf", sTmp, &fTmp);

	nTotal = header->ncols*header->nrows;
	value = (double *)MyMalloc(nTotal*sizeof(double));
	header->index = (int *)MyMalloc(nTotal*sizeof(int));
	if((sTmp[0]=='n')||(sTmp[0]=='N'))
	{
		header->isnodata = true;
		header->nodata_value = fTmp;
		for(i=0;i<nTotal;i++)
		{
			//fscanf(fp,"%lf", &fTmp);
			//value[i] = fTmp;
			fscanf(fp,"%lf", value+i);
		}
	}
	else
	{
		header->isnodata = false;
		value[1] = fTmp;
		//sscanf(sTmp,"%lf",&fTmp);
		//value[0] = fTmp;
		sscanf(sTmp,"%lf",value);
		for(i=2;i<nTotal;i++)
		{
			//fscanf(fp,"%lf", &fTmp);
			//value[i] = fTmp;
			fscanf(fp,"%lf", value+i);
		}
	}

	m_pf3Vertex = (FVECTOR3 *)MyMalloc(nTotal*sizeof(FVECTOR3));
	m_pn3Face = (NVECTOR3 *)MyMalloc(2*(header->ncols-1)*(header->nrows-1)*sizeof(NVECTOR3));
	if(header->isnodata)
	{
		m_nNumVertex = 0;
		for(i=0; i<header->nrows; i++)
		{
			for(j=0;j<header->ncols;j++)
			{
				k = j+i*header->ncols;
				if(abs(value[k]-header->nodata_value)<FLT_EPSILON)
				{
					header->index[k] =nTotal;
				}
				else
				{
					m_pf3Vertex[m_nNumVertex][0]=float(i*header->cellsize);
					m_pf3Vertex[m_nNumVertex][1]=float(j*header->cellsize);
					m_pf3Vertex[m_nNumVertex][2]=float(value[k]);
					header->index[k] =m_nNumVertex;
					m_nNumVertex++;
				}
			}
		}
		m_pf3Vertex = (FVECTOR3 *)MyRealloc(m_pf3Vertex, m_nNumVertex*sizeof(FVECTOR3));

		m_nNumFace = 0;
		for(i=0; i<header->nrows-1; i++)
		{
			for(j=0;j<header->ncols-1;j++)
			{
				kk[0] = j+i*header->ncols;
				kk[1] = kk[0]+1;
				kk[2] = kk[0]+header->ncols;
				kk[3] = kk[2]+1;
				k = 4;
				for(ii=0;ii<4;ii++){
					if(header->index[kk[ii]]==nTotal){
						if(k<ii){
							k=5;
							break;
						}else{
							k=ii;
						}
					}
				}
				if(k==0){
					m_pn3Face[m_nNumFace][0] = header->index[kk[1]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
					m_nNumFace++;
				}else if(k==1){
					m_pn3Face[m_nNumFace][0] = header->index[kk[0]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
					m_nNumFace++;
				}else if(k==2){
					m_pn3Face[m_nNumFace][0] = header->index[kk[1]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[0]];
					m_nNumFace++;
				}else if(k==3){
					m_pn3Face[m_nNumFace][0] = header->index[kk[0]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[1]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
					m_nNumFace++;
				}else if(k==4){//generate two triangles with minimum total area 
					if((abs(value[header->index[kk[2]]]-value[header->index[kk[0]]])> 
						abs(value[header->index[kk[3]]]-value[header->index[kk[1]]]))&& 
						(abs(value[header->index[kk[1]]]-value[header->index[kk[0]]])> 
						abs(value[header->index[kk[3]]]-value[header->index[kk[2]]])))
					{
						m_pn3Face[m_nNumFace][0] = header->index[kk[0]];
						m_pn3Face[m_nNumFace][1] = header->index[kk[1]];
						m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
						m_nNumFace++;
						m_pn3Face[m_nNumFace][0] = header->index[kk[1]];
						m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
						m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
						m_nNumFace++;
					}else{
						m_pn3Face[m_nNumFace][0] = header->index[kk[1]];
						m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
						m_pn3Face[m_nNumFace][2] = header->index[kk[0]];
						m_nNumFace++;
						m_pn3Face[m_nNumFace][0] = header->index[kk[0]];
						m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
						m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
						m_nNumFace++;
					}
				}
			}
		}
		m_pn3Face = (NVECTOR3 *)MyRealloc(m_pn3Face, m_nNumFace*sizeof(NVECTOR3));
	}
	else
	{
		m_nNumVertex = nTotal;
		for(i=0; i<header->nrows; i++)
		{
			for(j=0;j<header->ncols;j++)
			{
				k = j+i*header->ncols;
				m_pf3Vertex[k][0]=float(i*header->cellsize);
				m_pf3Vertex[k][1]=float(j*header->cellsize);
				m_pf3Vertex[k][2]=float(value[k]);
				header->index[k]=k;
			}
		}
		for(i=0; i<header->nrows-1; i++)
		{
			for(j=0;j<header->ncols-1;j++)
			{
				kk[0] = j+i*header->ncols;
				kk[1] = kk[0]+1;
				kk[2] = kk[0]+header->ncols;
				kk[3] = kk[2]+1;
				if((abs(value[header->index[kk[2]]]-value[header->index[kk[0]]])> 
					abs(value[header->index[kk[3]]]-value[header->index[kk[1]]]))&& 
					(abs(value[header->index[kk[1]]]-value[header->index[kk[0]]])> 
					abs(value[header->index[kk[3]]]-value[header->index[kk[2]]])))
				{
					m_pn3Face[m_nNumFace][0] = header->index[kk[0]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[1]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
					m_nNumFace++;
					m_pn3Face[m_nNumFace][0] = header->index[kk[1]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
					m_nNumFace++;
				}else{
					m_pn3Face[m_nNumFace][0] = header->index[kk[1]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[0]];
					m_nNumFace++;
					m_pn3Face[m_nNumFace][0] = header->index[kk[0]];
					m_pn3Face[m_nNumFace][1] = header->index[kk[3]];
					m_pn3Face[m_nNumFace][2] = header->index[kk[2]];
					m_nNumFace++;
				}
			}
		}
	}
    free(value);
}

void ScalingBox(void)
{
    int i,j;
    float box[2][3];

    box[0][0] = box[0][1] = box[0][2] = FLT_MAX;
    box[1][0] = box[1][1] = box[1][2] = -FLT_MAX;
    for (i=0;i<m_nNumVertex;i++)
    {
        for (j=0;j<3;j++)
        {
            if (box[0][j]>m_pf3Vertex[i][j])
                box[0][j] = m_pf3Vertex[i][j];
            if (box[1][j]<m_pf3Vertex[i][j])
                box[1][j] = m_pf3Vertex[i][j];
        }
    }
    m_f3Centre[0] = (box[0][0]+box[1][0])/2.0;
    m_f3Centre[1] = (box[0][1]+box[1][1])/2.0;
    m_f3Centre[2] = (box[0][2]+box[1][2])/2.0;

    m_fScale = FMAX(box[1][0]-box[0][0],FMAX(box[1][1]-box[0][1],box[1][2]-box[0][2]));
    m_fScale /=2.0;
    for (i=0;i<m_nNumVertex;i++)
    {
        VEC3_VOPV_OP_S(m_pf3Vertex[i],m_pf3Vertex[i],-,m_f3Centre,/,m_fScale);
    }
}

void ComputeNormal(bool bProduced)
{
    int i, j;
    FVECTOR3 vect[3];
    float fArea;
   
    if(bProduced)
    {
        if(m_pf3VertexNormalP != NULL)
            delete []m_pf3VertexNormalP;
        if(m_pf3FaceNormalP != NULL)
            delete []m_pf3FaceNormalP;

        m_pf3VertexNormalP = new FVECTOR3[m_nNumVertexP];
        m_pf3FaceNormalP = new FVECTOR3[m_nNumFaceP];
       
        for (i=0;i<m_nNumVertexP;i++)
        {
            VEC3_ZERO(m_pf3VertexNormalP[i]);
        }
        for (i=0;i<m_nNumFaceP;i++) // compute each triangle normal and vertex normal
        {
            VEC3_V_OP_V(vect[0],m_pf3VertexP[m_pn3FaceP[i][1]],-,m_pf3VertexP[m_pn3FaceP[i][0]]);
            VEC3_V_OP_V(vect[1],m_pf3VertexP[m_pn3FaceP[i][2]],-,m_pf3VertexP[m_pn3FaceP[i][0]]);
            CROSSPROD3(vect[2],vect[0],vect[1]);
            fArea = sqrt(DOTPROD3(vect[2], vect[2]))/2.0f; // Area of the face
            V3Normalize(vect[2]);
            VEC3_ASN_OP(m_pf3FaceNormalP[i],=,vect[2]);
            for (j=0;j<3;j++)
            {
                VEC3_V_OP_V_OP_S(m_pf3VertexNormalP[m_pn3FaceP[i][j]], \
                    m_pf3VertexNormalP[m_pn3FaceP[i][j]], +, vect[2], *, fArea);
            }
        }
        for (i=0;i<m_nNumVertexP;i++)
            V3Normalize(m_pf3VertexNormalP[i]);
    }
    else
    {   
        if(m_pf3VertexNormal != NULL)
            delete []m_pf3VertexNormal;
        if(m_pf3FaceNormal != NULL)
            delete []m_pf3FaceNormal;

        m_pf3VertexNormal = new FVECTOR3[m_nNumVertex];
        m_pf3FaceNormal = new FVECTOR3[m_nNumFace];
       
        for (i=0;i<m_nNumVertex;i++)
        {
            VEC3_ZERO(m_pf3VertexNormal[i]);
        }
        for (i=0;i<m_nNumFace;i++) // compute each triangle normal and vertex normal
        {
            VEC3_V_OP_V(vect[0],m_pf3Vertex[m_pn3Face[i][1]],-,m_pf3Vertex[m_pn3Face[i][0]]);
            VEC3_V_OP_V(vect[1],m_pf3Vertex[m_pn3Face[i][2]],-,m_pf3Vertex[m_pn3Face[i][0]]);
            CROSSPROD3(vect[2],vect[0],vect[1]);
            fArea = sqrt(DOTPROD3(vect[2], vect[2]))/2.0f; // Area of the face
            V3Normalize(vect[2]);
            VEC3_ASN_OP(m_pf3FaceNormal[i],=,vect[2]);
            for (j=0;j<3;j++)
            {
                VEC3_V_OP_V_OP_S(m_pf3VertexNormal[m_pn3Face[i][j]], \
                    m_pf3VertexNormal[m_pn3Face[i][j]], +, vect[2], *, fArea);
            }
        }
        for (i=0;i<m_nNumVertex;i++)
            V3Normalize(m_pf3VertexNormal[i]);
    }
}

void V3Normalize(FVECTOR3 v)
{
    float len;
    len=sqrt(DOTPROD3(v,v));
    if (len!=0.0)
    {
        v[0]=v[0]/len;
        v[1]=v[1]/len;
        v[2]=v[2]/len;
    }
}

void MeshDenoise(bool bNeighbourCV, float fSigma, int nIterations, int nVIterations)
{
    int **ttRing; //store the list of triangle neighbours of a triangle

    FVECTOR3 *Vertex;
    FVECTOR3 *TNormal;

    int i,k,m;
    float tmp3;

    if (m_nNumFace == 0)
        return;

    delete []m_pf3VertexP;
    delete []m_pf3VertexNormalP;
    delete []m_pf3FaceNormalP;
    ComputeVRing1V(); //find the neighbouring vertices of each vertex
    ComputeVRing1T();     //find the neighbouring triangles of each vertex

    //find out the neighbouring triangles of each triangle
    if (bNeighbourCV)
    {
        ComputeTRing1TCV();
        ttRing = m_ppnTRing1TCV;
        for (k=0; k<m_nNumFace; k++)
        {
            ttRing[k] = m_ppnTRing1TCV[k];       
        }
    }
    else
    {
        ComputeTRing1TCE();
        ttRing = m_ppnTRing1TCE;
        for (k=0; k<m_nNumFace; k++)
        {
            ttRing[k] = m_ppnTRing1TCE[k];       
        }
    }

    //begin filter
    m_nNumVertexP = m_nNumVertex;
    m_nNumFaceP = m_nNumFace;
    m_pf3VertexP = new FVECTOR3[m_nNumVertexP];
    m_pf3FaceNormalP = new FVECTOR3[m_nNumFaceP];
    m_pf3VertexNormalP = new FVECTOR3[m_nNumVertexP];
    Vertex = new FVECTOR3[m_nNumVertexP];
    TNormal = new FVECTOR3[m_nNumFace];
    for(i=0; i<m_nNumFace; i++)
    {
        VEC3_ASN_OP(m_pf3FaceNormalP[i], =, m_pf3FaceNormal[i]);
    }
    for(i=0; i<m_nNumVertex; i++)
    {
        VEC3_ASN_OP(m_pf3VertexP[i], =, m_pf3Vertex[i]);
    }

    for(i=0; i<m_nNumVertex; i++)
    {
        VEC3_ASN_OP(Vertex[i], =, m_pf3VertexP[i]);
    }

    for(m=0; m<nIterations; m++)
    {
        //initialization
        for(i=0; i<m_nNumFace; i++)
        {
            VEC3_ASN_OP(TNormal[i], =, m_pf3FaceNormalP[i]);
        }

        //modify triangle normal
        for(k=0; k<m_nNumFace; k++)
        {
            VEC3_ZERO(m_pf3FaceNormalP[k]);
            for(i=1; i<ttRing[k][0]+1; i++)
            {
                tmp3 = DOTPROD3(TNormal[ttRing[k][i]],TNormal[k])-fSigma;
                if( tmp3 > 0.0)
                {
                    VEC3_V_OP_V_OP_S(m_pf3FaceNormalP[k],m_pf3FaceNormalP[k], +, TNormal[ttRing[k][i]], *, tmp3*tmp3);
                }
            }
            V3Normalize(m_pf3FaceNormalP[k]);
        }
        for(k=0; k<m_nNumFace; k++)
        {
            VEC3_ASN_OP(TNormal[k], =, m_pf3FaceNormalP[k]);
        }
    }

    //modify vertex coordinates
    VertexUpdate(m_ppnVRing1T, nVIterations);
    //m_L2Error = L2Error();

    delete []Vertex;
    delete []TNormal;

    return;
}

void ComputeVRing1V(void)
{
    int i,j,k;
    int tmp0, tmp1, tmp2;

    if(m_ppnVRing1V != NULL)
        return;

    m_ppnVRing1V=(int **)MyMalloc(m_nNumVertex*sizeof(int *));

    for (i=0;i<m_nNumVertex;i++) {
        m_ppnVRing1V[i]=(int *)MyMalloc(6*sizeof(int));
        m_ppnVRing1V[i][0]=0; // m_ppnVRing1V[i][0] stores the number of vertex neighbours
    }

    for (k=0; k<m_nNumFace; k++)
    {
        for (i=0;i<3;i++)
        {
            tmp0=m_pn3Face[k][i];
            tmp2=m_pn3Face[k][(i+2)%3];
            for (j=1;j<m_ppnVRing1V[tmp0][0]+1;j++)
                if (m_ppnVRing1V[tmp0][j] == tmp2)
                    break;
            if (j==m_ppnVRing1V[tmp0][0]+1)
            {
                m_ppnVRing1V[tmp0][j]=tmp2;
                m_ppnVRing1V[tmp0][0] += 1;
                if (!(m_ppnVRing1V[tmp0][0]%5))
                    m_ppnVRing1V[tmp0] = (int *)MyRealloc(m_ppnVRing1V[tmp0],(m_ppnVRing1V[tmp0][0]+6)*sizeof(int));
            }
            tmp1=m_pn3Face[k][(i+1)%3];
            for (j=1;j<m_ppnVRing1V[tmp0][0]+1;j++)
                if (m_ppnVRing1V[tmp0][j] == tmp1)
                    break;
            if (j==m_ppnVRing1V[tmp0][0]+1)
            {
                m_ppnVRing1V[tmp0][j]=tmp1;
                m_ppnVRing1V[tmp0][0] += 1;
                if (!(m_ppnVRing1V[tmp0][0]%5))
                    m_ppnVRing1V[tmp0] = (int *)MyRealloc(m_ppnVRing1V[tmp0],(m_ppnVRing1V[tmp0][0]+6)*sizeof(int));
            }   
        }
    }
    for (i=0;i<m_nNumVertex;i++) {
        m_ppnVRing1V[i]=(int *)MyRealloc(m_ppnVRing1V[i],(m_ppnVRing1V[i][0]+1)*sizeof(int));
    }
}

void ComputeVRing1T(void)
{
        int i,k;
    int tmp;

    if(m_ppnVRing1T != NULL)
        return;

    m_ppnVRing1T=(int **)MyMalloc(m_nNumVertex*sizeof(int *));
    for (i=0;i<m_nNumVertex;i++) {
        m_ppnVRing1T[i]=(int *)MyMalloc(6*sizeof(int));
        m_ppnVRing1T[i][0]=0; // m_ppnVRing1T[i][0] stores the number of triangle neighbours
    }

    for (k=0; k<m_nNumFace; k++)
    {
        for (i=0;i<3;i++)
        {
            tmp = m_pn3Face[k][i]; //the vertex incident to the k-th triangle
            m_ppnVRing1T[tmp][0] += 1;
            m_ppnVRing1T[tmp][m_ppnVRing1T[tmp][0]]=k;
            if (!(m_ppnVRing1T[tmp][0]%5))
            {
                m_ppnVRing1T[tmp] = (int *)MyRealloc(m_ppnVRing1T[tmp],(m_ppnVRing1T[tmp][0]+6)*sizeof(int));
            }
        }
    }
    for (i=0;i<m_nNumVertex;i++) {
        m_ppnVRing1T[i]=(int *)MyRealloc(m_ppnVRing1T[i],(m_ppnVRing1T[i][0]+1)*sizeof(int));
    }
}

void ComputeTRing1TCV(void)
{
    int i,j,k;
    int tmp,tmp0,tmp1,tmp2;

    if(m_ppnTRing1TCV != NULL)
        return;

    m_ppnTRing1TCV=(int **)MyMalloc(m_nNumFace*sizeof(int *));
    for (k=0; k<m_nNumFace; k++)
    {
        tmp0 = m_pn3Face[k][0]; 
        tmp1 = m_pn3Face[k][1];
        tmp2 = m_pn3Face[k][2];
        tmp = m_ppnVRing1T[tmp0][0] + m_ppnVRing1T[tmp1][0] +  m_ppnVRing1T[tmp2][0];
        m_ppnTRing1TCV[k] =(int *)MyMalloc(tmp*sizeof(int));
       
        m_ppnTRing1TCV[k][0] = m_ppnVRing1T[tmp0][0];
        for (i=1; i<m_ppnVRing1T[tmp0][0]+1; i++)
        {
            m_ppnTRing1TCV[k][i] = m_ppnVRing1T[tmp0][i];
        }

        for (i=1; i<m_ppnVRing1T[tmp1][0]+1; i++)
        {
            if((m_pn3Face[m_ppnVRing1T[tmp1][i]][0] != tmp0) && (m_pn3Face[m_ppnVRing1T[tmp1][i]][1] != tmp0)\
                && (m_pn3Face[m_ppnVRing1T[tmp1][i]][2] != tmp0))
            {
                ++m_ppnTRing1TCV[k][0];
                m_ppnTRing1TCV[k][m_ppnTRing1TCV[k][0]]= m_ppnVRing1T[tmp1][i];
            }
            else
            {
                for(j=1; j<m_ppnTRing1TCV[k][0]+1; j++)
                {
                    if(m_ppnTRing1TCV[k][j]==m_ppnVRing1T[tmp1][i])
                    {
                        break;
                    }
                }
            }
        }

        for (i=1; i<m_ppnVRing1T[tmp2][0]+1; i++)
        {
            if((m_pn3Face[m_ppnVRing1T[tmp2][i]][0] != tmp0) && (m_pn3Face[m_ppnVRing1T[tmp2][i]][1] != tmp0)\
                && (m_pn3Face[m_ppnVRing1T[tmp2][i]][2] != tmp0) && (m_pn3Face[m_ppnVRing1T[tmp2][i]][0] != tmp1)\
                && (m_pn3Face[m_ppnVRing1T[tmp2][i]][1] != tmp1) && (m_pn3Face[m_ppnVRing1T[tmp2][i]][2] != tmp1))
            {
                ++m_ppnTRing1TCV[k][0];
                m_ppnTRing1TCV[k][m_ppnTRing1TCV[k][0]]= m_ppnVRing1T[tmp2][i];
            }
            else
            {
                for(j=1; j<m_ppnTRing1TCV[k][0]+1; j++)
                {
                    if(m_ppnTRing1TCV[k][j]==m_ppnVRing1T[tmp2][i])
                    {
                        break;
                    }
                }
            }
        }
    }
    for (i=0;i<m_nNumFace;i++) {
        m_ppnTRing1TCV[i]=(int *)MyRealloc(m_ppnTRing1TCV[i],(m_ppnTRing1TCV[i][0]+1)*sizeof(int));
    }
}

void ComputeTRing1TCE(void)
{
    int i,k;
    int tmp,tmp0,tmp1,tmp2;

    if(m_ppnTRing1TCE != NULL)
        return;

    m_ppnTRing1TCE=(int **)MyMalloc(m_nNumFace*sizeof(int *));
    for (k=0; k<m_nNumFace; k++)
    {
        tmp0 = m_pn3Face[k][0]; 
        tmp1 = m_pn3Face[k][1];
        tmp2 = m_pn3Face[k][2];

        m_ppnTRing1TCE[k] = (int *)MyMalloc(5*sizeof(int));

        tmp = 0;
        for (i=1; i<m_ppnVRing1T[tmp0][0]+1; i++)
        {
            if ((m_pn3Face[m_ppnVRing1T[tmp0][i]][0] == tmp1)||(m_pn3Face[m_ppnVRing1T[tmp0][i]][0] == tmp2)||\
                    (m_pn3Face[m_ppnVRing1T[tmp0][i]][1] == tmp1)||(m_pn3Face[m_ppnVRing1T[tmp0][i]][1] == tmp2)||\
                    (m_pn3Face[m_ppnVRing1T[tmp0][i]][2] == tmp1)||(m_pn3Face[m_ppnVRing1T[tmp0][i]][2] == tmp2))
            {
                tmp++;
                if (tmp>4)
                {
                    tmp--;
                    break;
                }
                m_ppnTRing1TCE[k][tmp] = m_ppnVRing1T[tmp0][i];
            }
        }

        for (i=1; i<m_ppnVRing1T[tmp1][0]+1; i++)
        {
            if ((m_pn3Face[m_ppnVRing1T[tmp1][i]][0] == tmp1)&&\
                ((m_pn3Face[m_ppnVRing1T[tmp1][i]][1] == tmp2)||(m_pn3Face[m_ppnVRing1T[tmp1][i]][2] == tmp2)))
            {
                tmp++;
                if (tmp>4)
                {
                    tmp--;
                    break;
                }
                m_ppnTRing1TCE[k][tmp] = m_ppnVRing1T[tmp1][i];
                break;   
            }
            else if((m_pn3Face[m_ppnVRing1T[tmp1][i]][0] == tmp2)&&\
                ((m_pn3Face[m_ppnVRing1T[tmp1][i]][1] == tmp1)||(m_pn3Face[m_ppnVRing1T[tmp1][i]][2] == tmp1)))
            {
                tmp++;
                if (tmp>4)
                {
                    tmp--;
                    break;
                }
                m_ppnTRing1TCE[k][tmp] = m_ppnVRing1T[tmp1][i];
                break;   
            }
            else if((m_pn3Face[m_ppnVRing1T[tmp1][i]][1] == tmp2)&&(m_pn3Face[m_ppnVRing1T[tmp1][i]][2] == tmp1))
            {
                tmp++;
                if (tmp>4)
                {
                    tmp--;
                    break;
                }
                m_ppnTRing1TCE[k][tmp] = m_ppnVRing1T[tmp1][i];
                break;   
            }
            else if((m_pn3Face[m_ppnVRing1T[tmp1][i]][1] == tmp1)&&\
                (m_pn3Face[m_ppnVRing1T[tmp1][i]][2] == tmp2)&&(m_pn3Face[m_ppnVRing1T[tmp1][i]][0] != tmp0))
            {
                tmp++;
                if (tmp>4)
                {
                    tmp--;
                    break;
                }
                m_ppnTRing1TCE[k][tmp] = m_ppnVRing1T[tmp1][i];
                break;   
            }
        }
        m_ppnTRing1TCE[k][0] = tmp;
    }
}

void VertexUpdate(int** tRing, int nVIterations)
{
    int i, j, m;
    int nTmp0, nTmp1, nTmp2;
    float fTmp1;

    FVECTOR3 vect[3];
   
    for(m=0; m<nVIterations; m++)
    {   

        for(i=0; i<m_nNumVertex; i++)
        {
            VEC3_ZERO(vect[1]);
            for(j=1; j<tRing[i][0]+1; j++)
            {
                nTmp0 = m_pn3Face[tRing[i][j]][0]; // the vertex number of triangle tRing[i][j]
                nTmp1 = m_pn3Face[tRing[i][j]][1];
                nTmp2 = m_pn3Face[tRing[i][j]][2];
                VEC3_V_OP_V_OP_V(vect[0], m_pf3VertexP[nTmp0],+, m_pf3VertexP[nTmp1],+, m_pf3VertexP[nTmp2]);
                VEC3_V_OP_S(vect[0], vect[0], /, 3.0); //vect[0] is the centr of the triangle.
                VEC3_V_OP_V(vect[0], vect[0], -, m_pf3VertexP[i]); //vect[0] is now vector PC.
                fTmp1 = DOTPROD3(vect[0], m_pf3FaceNormalP[tRing[i][j]]);
				if(m_bZOnly)
					vect[1][2] = vect[1][2] + m_pf3FaceNormalP[tRing[i][j]][2] * fTmp1;
				else
					VEC3_V_OP_V_OP_S(vect[1], vect[1], +, m_pf3FaceNormalP[tRing[i][j]],*, fTmp1);                   
            }
            if (tRing[i][0]!=0)
            {
				if(m_bZOnly)
					m_pf3VertexP[i][2] = m_pf3VertexP[i][2] + vect[1][2]/tRing[i][0];
				else
					VEC3_V_OP_V_OP_S(m_pf3VertexP[i], m_pf3VertexP[i],+, vect[1], /, tRing[i][0]);
            }
        }
    }
    ComputeNormal(TRUE);
}

void SaveData(FILE * fp, int nfileext, struct ESRIHeader* header)
{
    for (int i=0;i<m_nNumVertexP;i++)
    {
        VEC3_V_OP_V_OP_S(m_pf3VertexP[i],m_f3Centre,+, m_pf3VertexP[i],*, m_fScale);
    }

    switch (nfileext)
    {
/*    case FILE_GTS:
        SaveGTS(fp);
        break;
*/
    case FILE_OBJ:
        SaveOBJ(fp);
        break;

    case FILE_OFF:
        SaveOFF(fp);
        break;

    case FILE_PLY:
        SavePLY(fp);
        break;

    case FILE_PLY2:
        SavePLY2(fp);
        break;

/*    case FILE_SMF:
        SaveSMF(fp);
        break;

    case FILE_STL:
        SaveSTL(fp);
        break;

    case FILE_WRL:
        SaveWRL(fp);
        break;
*/
    case FILE_XYZ:
        SaveXYZ(fp);
        break;

	case FILE_ESRI:
        SaveESRI(fp, header);
        break;

    default:
        SaveOFF(fp);
        return;
    }
}

void SaveOBJ(FILE * fp)
{
    int i;

    fprintf(fp,"# The denoised result.\n");

    for (i=0;i<m_nNumVertexP;i++)
    {
        fprintf(fp,"v %f %f %f\n", m_pf3VertexP[i][0], m_pf3VertexP[i][1], m_pf3VertexP[i][2]);
    }
    for (i=0;i<m_nNumFaceP;i++)
    {
        fprintf(fp,"f %d %d %d\n", m_pn3FaceP[i][0]+1, m_pn3FaceP[i][1]+1, m_pn3FaceP[i][2]+1);
    }
}

void SaveOFF(FILE * fp)
{
    int i;

    fprintf(fp,"OFF\n");
    fprintf(fp,"%d %d %d\n",m_nNumVertexP,m_nNumFaceP, 0);

    for (i=0;i<m_nNumVertexP;i++)
    {
        fprintf(fp,"%f %f %f\n", m_pf3VertexP[i][0], m_pf3VertexP[i][1], m_pf3VertexP[i][2]);
    }
    for (i=0;i<m_nNumFaceP;i++)
    {
        fprintf(fp,"3 %d %d %d\n", m_pn3FaceP[i][0], m_pn3FaceP[i][1], m_pn3FaceP[i][2]);
    }
}

void SavePLY(FILE * fp)
{
    int i;

    fprintf(fp,"ply\n");
    fprintf(fp,"format ascii 1.0\n");
    fprintf(fp,"comment The denoised result.\n");
    fprintf(fp,"element vertex %d\n", m_nNumVertexP);
    fprintf(fp,"property float x\n");
    fprintf(fp,"property float y\n");
    fprintf(fp,"property float z\n");
    fprintf(fp,"element face %d\n", m_nNumFaceP);
    fprintf(fp,"property list uchar int vertex_indices\n");
    fprintf(fp,"end_header\n");

    for (i=0;i<m_nNumVertexP;i++)
    {
        fprintf(fp,"%f %f %f\n", m_pf3VertexP[i][0], m_pf3VertexP[i][1], m_pf3VertexP[i][2]);
    }
    for (i=0;i<m_nNumFaceP;i++)
    {
        fprintf(fp,"3 %d %d %d\n", m_pn3FaceP[i][0], m_pn3FaceP[i][1], m_pn3FaceP[i][2]);
    }
}

void SavePLY2(FILE * fp)
{
    int i;

    fprintf(fp,"%d\n",m_nNumVertexP);
    fprintf(fp,"%d\n",m_nNumFaceP);

    for (i=0;i<m_nNumVertexP;i++)
    {
        fprintf(fp,"%f %f %f\n", m_pf3VertexP[i][0], m_pf3VertexP[i][1], m_pf3VertexP[i][2]);
    }
    for (i=0;i<m_nNumFaceP;i++)
    {
        fprintf(fp,"3 %d %d %d\n", m_pn3FaceP[i][0], m_pn3FaceP[i][1], m_pn3FaceP[i][2]);
    }
}

void SaveXYZ(FILE * fp)
{
    int i;
    //fprintf(fp,"%d\n",m_nNumVertexP);

    for (i=0;i<m_nNumVertexP;i++)
    {
        fprintf(fp,"%f %f %f\n", m_pf3VertexP[i][0], m_pf3VertexP[i][1], m_pf3VertexP[i][2]);
    }
}

void SaveESRI(FILE * fp, ESRIHeader* header)
{
    int i,j,k,nTotal;
    fprintf(fp,"ncols          %d\n",header->ncols);
    fprintf(fp,"nrows          %d\n",header->nrows);
    fprintf(fp,"xllcorner      %lf\n",header->xllcorner);
    fprintf(fp,"yllcorner      %lf\n",header->yllcorner);
    fprintf(fp,"cellsize       %lf\n",header->cellsize);

	nTotal = header->nrows*header->ncols;
	if(header->isnodata){
		fprintf(fp,"NODATA_value   %lf\n",header->nodata_value);
		for(i=0;i<header->nrows;i++)
		{
			for(j=0;j<header->ncols;j++){
				k = j+i*header->ncols;
				k = header->index[k];
				if(k==nTotal){
					fprintf(fp,"%lf ", header->nodata_value);
				}else{
					fprintf(fp,"%lf ", m_pf3VertexP[k][2]);
				}
			}
			fprintf(fp,"\n");
		}
	}
	else{
		for(i=0;i<header->nrows;i++)
		{
			for(j=0;j<header->ncols;j++){
				k = j+i*header->ncols;
				fprintf(fp,"%lf ", m_pf3VertexP[k][2]);
			}
			fprintf(fp,"\n");
		}
	}
}

void options(char *progname)
{
    printf("usage: %s -i input_file [options]\n",progname);
    printf("     -e         Common Edge Type of Face Neighbourhood (Default: Common Vertex)\n");
    printf("     -t float   Threshold (0,1), Default value: 0.4\n");
    printf("     -n int     Number of Iterations for Normal updating, Default value: 20\n");
    printf("     -v int     Number of Iterations for Vertex updating, Default value: 50\n");
    printf("     -o char[]  Output file\n");
    printf("     -a         Adds edges and vertices to generate high-quality triangle mesh\n");
    printf("                Only functions when the input is .xyz file\n");
    printf("     -z         Only z-direction position is updated\n\n");
    printf("Supported input type: .gts, .obj, .off, .ply, .ply2, .smf, .stl, .wrl, .xyz, and .asc\n");
    printf("Supported output type: .obj, .off, .ply, .ply2, .xyz, and .asc\n");
    printf("Default file extension: .off\n\n");
    printf("Examples:\n");
    printf("%s -i cylinderN02.ply2\n",progname);
    printf("%s -i cylinderN02.ply2 -n 5 -o cylinderDN\n",progname);
    printf("%s -i cylinderN02.ply2 -t 0.8 -e -v 20 -o cylinderDN.obj\n",progname);
    printf("%s -i FandiskNI02-05 -o FandiskDN.ply\n",progname);
    printf("%s -i Terrain.xyz -o TerrainP -z -n 1\n",progname);
    printf("%s -i my_dem_utm.asc -o my_dem_utmP -n 4\n",progname);

   exit(-1);
}

/*
int *pI;
try { pI = new int; }
catch(std::bad_alloc) {
    cout << "ERROR: no more memory\n";
    return(1);
}
delete pI;
*/
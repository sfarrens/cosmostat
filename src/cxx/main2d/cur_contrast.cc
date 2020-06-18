/******************************************************************************
**                   Copyright (C) 2000 by CEA
*******************************************************************************
**
**    UNIT
**
**    Version: 1.0
**
**    Author: Jean-Luc Starck
**
**    Date:  03/02/00
**    
**    File:  cur_trans.cc
**
*******************************************************************************
**
**    DESCRIPTION  contrast enhancement by the curvelet transform
**    ----------- 
**                 
******************************************************************************/

#include "IM_Obj.h"
#include "IM_IO.h"
#include "FFTN_2D.h"
#include "IM_Math.h"
#include "MR_Contrast.h"
#include "Curvelet.h"
#include "CurContrast.h"

char Name_Imag_In[256];  // input file image  
char Name_Imag_Out[256]; //  output file name  
 
extern int  OptInd;
extern char *OptArg;

extern int  GetOpt(int argc, char **argv, char *opts);

Bool Verbose=False; 
Bool Reverse=False; // Reverse transform
int NbrScale2D = DEF_CUR_NBR_SCALE_2D;
int NbrScale1D = -1;
int BlockSize= DEF_CUR_BLOCK_SIZE;
type_ridgelet_WTtrans RidTrans = DEF_RID_TRANS;
Bool BlockOverlap = False;
float Noise_Ima=0.;
float N_Sigma=DEFAULT_N_SIGMA;
Bool Clip = False;
float NSigmaLow=3.;
float NSigmaUp=20.;
float ClipVal=3.;
Bool UseSigmaUp=False;
double QParam = 0.;
double CParam = 0.;  // must be the noise level
double PParam = 0.5; // P determined the degree of non lineraity
double MParam = 0.5; // values larger than m are not modified

float L_Coeff=1.;
float SParam=0.;
Bool UseLog = False;
Bool PosIma = True;
Bool UseVeldeFunction = False;

/***************************************/

static void usage(char *argv[])
{
    // int i;
    fprintf(OUTMAN, "Usage: %s options in_image result\n\n", argv[0]);
    fprintf(OUTMAN, "   where options =  \n");

    fprintf(OUTMAN, "         [-n number_of_scales]\n");
    fprintf(OUTMAN, "             Number of scales used in the wavelet transform.\n");
    fprintf(OUTMAN, "             default is %d. \n", NbrScale2D);    
    manline();

    fprintf(OUTMAN, "         [-N number_of_scales]\n");
    fprintf(OUTMAN, "             Number of scales used in the ridgelet transform.\n");
    fprintf(OUTMAN, "             default is automatically calculated. \n");    
    manline();
    
    fprintf(OUTMAN, "         [-b BlockSize]\n");
    fprintf(OUTMAN, "             Block Size.\n");
    fprintf(OUTMAN, "             default is %d. \n", BlockSize);    
    manline();

    fprintf(OUTMAN, "         [-O]\n");
    fprintf(OUTMAN, "             Use overlapping block. Default is no. \n");    
    manline();

    gauss_usage();
    manline();
    
    fprintf(OUTMAN, "         [-s NSigmalLow]\n");
    fprintf(OUTMAN, "             Coefficient < NSigmalLow*SigmaNoise are not modified.\n");
    fprintf(OUTMAN, "             default is %5.1f. \n", NSigmaLow);
    manline();    

    fprintf(OUTMAN, "         [-S NSigmalUp]\n");
    fprintf(OUTMAN, "             Coefficient > NSigmalUp*SigmaNoise are not modified.\n");
    fprintf(OUTMAN, "             default is %5.1f. \n", NSigmaUp);
    manline();

    fprintf(OUTMAN, "         [-M MaxCoeff]\n");
    fprintf(OUTMAN, "             Coefficient > MaxBandCoef*MaxCoeff are not modified.\n");
    fprintf(OUTMAN, "             default is %5.1f. \n", MParam);
    manline();
        
    fprintf(OUTMAN, "         [-P P_parameter. P in ]0,1[]\n");
    fprintf(OUTMAN, "             Determine the degree on non-linearity. \n");
    fprintf(OUTMAN, "             P must be in ]0,1[.  \n");
    fprintf(OUTMAN, "             Default is %f.\n", PParam);
    manline();
    
    fprintf(OUTMAN, "         [-T P_parameter. T in ]0,1[)]\n");
    fprintf(OUTMAN, "             Curvelet coefficent saturation parameter. \n");
    fprintf(OUTMAN, "             T must be in [0,1].  \n");
    fprintf(OUTMAN, "             Default is %2.2f.\n", SParam);
    manline();
        
    fprintf(OUTMAN, "         [-c]\n");
    fprintf(OUTMAN, "             K-sigma clipping. Default is False.\n");
    manline();

    fprintf(OUTMAN, "         [-K ClippingValue]\n");
    fprintf(OUTMAN, "             Clipping value. Default is 3.\n");
    manline();

    fprintf(OUTMAN, "         [-L Saturation]\n");
    fprintf(OUTMAN, "             Saturate the reconstructed image.\n");
    fprintf(OUTMAN, "             Coeff larger than L*Max are set to L*Max.\n");
    fprintf(OUTMAN, "             default is %5.2f. \n", L_Coeff);
    fprintf(OUTMAN, "             If L is set to 0, then no saturation is applied.\n");
    manline();       
 
    fprintf(OUTMAN, "         [-p]\n");
    fprintf(OUTMAN, "             Remove positivity contraint.\n");
    manline();
    
    vm_usage();
    manline();    
    verbose_usage();
    manline();         
    manline();
    exit(-1);
}
  
/*********************************************************************/

/* GET COMMAND LINE ARGUMENTS */
static void filtinit(int argc, char *argv[])
{
    int c;
    float Val;
#ifdef LARGE_BUFF
    int VMSSize=-1;
    Bool OptZ = False;
    char VMSName[1024] = "";
#endif     

    /* get options */
    while ((c = GetOpt(argc,argv,"xpXT:P:Q:L:M:K:cg:s:S:Ob:n:N:vzZ")) != -1) 
    {
	switch (c) 
        {       
	  case 'x': UseVeldeFunction = True; break;
	  case 'p': PosIma = False; break; 
	  case 'X': UseLog = True; break; 
	  case 'T': 
	        if (sscanf(OptArg,"%f",&SParam) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad saturation param: %s\n", OptArg);
                    exit(-1);
                }

                if ((SParam < 0) || (SParam >= 1))
                {
                    fprintf(OUTMAN, "Error: bad saturation  param: %s\n", OptArg);
                    fprintf(OUTMAN, "0 <= L_Coef <= 1  \n");
                    exit(-1);
                }
                break;
	  case 'L': 
	        // if (sscanf(OptArg,"%f",&SParam) != 1) 
	        if (sscanf(OptArg,"%f",&L_Coeff) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad L_Coef param: %s\n", OptArg);
                    exit(-1);
                }

                if (L_Coeff < 0) 
                {
                    fprintf(OUTMAN, "Error: bad L_Coef param: %s\n", OptArg);
                    fprintf(OUTMAN, "0 <= L_Coef <= 1  \n");
                    exit(-1);
                }
                break;
 	  case 'K': 
                if (sscanf(OptArg,"%f",&ClipVal) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad clipping param: %s\n", OptArg);
                    exit(-1);
                }
                if (MParam <= 0)  
                {
                    fprintf(OUTMAN, "Error: bad clipping param: %s\n", OptArg);
                    fprintf(OUTMAN, "0 < MParam  \n");
                    exit(-1);
                }
                break;
           case 'c': Clip = (Clip==True) ? False: True; break;
	   case 's': 
               /* -n <Nbr_Plan> */
                if (sscanf(OptArg,"%f",&NSigmaLow) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad Nsigma value: %s\n", OptArg);
                    exit(-1);
                }
                if (NSigmaLow < 0)  
                {
                    fprintf(OUTMAN, "Error: bad Nsigma: %s\n", OptArg);
                    fprintf(OUTMAN, "0 <= Nsigma  \n");
                    exit(-1);
                }
                break;
	   case 'S': 
               /* -n <Nbr_Plan> */
                if (sscanf(OptArg,"%f",&NSigmaUp) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad Nsigma value: %s\n", OptArg);
                    exit(-1);
                }
                if (NSigmaUp < 0)  
                {
                    fprintf(OUTMAN, "Error: bad Nsigma: %s\n", OptArg);
                    fprintf(OUTMAN, "0 <= Nsigma  \n");
                    exit(-1);
                }
		UseSigmaUp = True;
                break;
	   case 'Q': 
               /* -n <Nbr_Plan> */
                if (sscanf(OptArg,"%f",&Val) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad QParam: %s\n", OptArg);
                    exit(-1);
                }
		QParam = Val;
                if ((QParam < -0.5) || (QParam > 0.5)) 
                {
                    fprintf(OUTMAN, "Error: bad QParam: %s\n", OptArg);
                    fprintf(OUTMAN, "-0.5 <= QParam <= 0.5 \n");
                    exit(-1);
                }
                break;
	   case 'P': 
               /* -n <Nbr_Plan> */
                if (sscanf(OptArg,"%f",&Val) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad PParam: %s\n", OptArg);
                    exit(-1);
                }
		PParam = Val;
                if ((PParam < 0) || (PParam > 1)) 
                {
                    fprintf(OUTMAN, "Error: bad PParam: %s\n", OptArg);
                    fprintf(OUTMAN, "0 < PParam < 1 \n");
                    exit(-1);
                }
                break;
	  case 'M': 
               /* -n <Nbr_Plan> */
                if (sscanf(OptArg,"%f",&Val) != 1) 
                {
                    fprintf(OUTMAN, "Error: bad MParam: %s\n", OptArg);
                    exit(-1);
                }
		MParam = Val;
                if (MParam <= 0)   
                {
                    fprintf(OUTMAN, "Error: bad MParam: %s\n", OptArg);
                    fprintf(OUTMAN, "0 < MParam  \n");
                    exit(-1);
                }
                break;
	  case 'g':
                /* -g <sigma_noise> */
                if (sscanf(OptArg,"%f",&Noise_Ima) != 1) 
                {
                    fprintf(OUTMAN, "bad sigma noise: %s\n", OptArg);
                    usage(argv);
                }
                break;
           case 'O': BlockOverlap= (BlockOverlap==True) ? False: True;break;
           case 'v': Verbose = True;break;
	   case 'b':
                if (sscanf(OptArg,"%d",&BlockSize) != 1) 
                {
                    fprintf(OUTMAN, "bad block size: %s\n", OptArg);
                    exit(-1);
                }
                break;
	   case 'n':
                /* -n <NbrScale2D> */
                if (sscanf(OptArg,"%d",&NbrScale2D) != 1) 
                {
                    fprintf(OUTMAN, "bad number of scales: %s\n", OptArg);
                    exit(-1);
                }
                if (NbrScale2D > MAX_SCALE)
                 {
                    fprintf(OUTMAN, "bad number of scales: %s\n", OptArg);
                    fprintf(OUTMAN, " Nbr Scales <= %d\n", MAX_SCALE);
                    exit(-1);
                }
                break;
	   case 'N':
                 if (sscanf(OptArg,"%d",&NbrScale1D) != 1) 
                {
                    fprintf(OUTMAN, "bad number of scales: %s\n", OptArg);
                    exit(-1);
                }
                break; 
#ifdef LARGE_BUFF
	    case 'z':
	        if (OptZ == True)
		{
                   fprintf(OUTMAN, "Error: Z option already set...\n");
                   exit(-1);
                }
	        OptZ = True;
	        break;
            case 'Z':
	        if (sscanf(OptArg,"%d:%s",&VMSSize, VMSName) < 1)
		{
		   fprintf(OUTMAN, "Error: syntaxe is Size:Directory ... \n");
		   exit(-1);
		}
	        if (OptZ == True)
		{
                   fprintf(OUTMAN, "Error: z option already set...\n");
                   exit(-1);
                }
		OptZ = True;
                break;
#endif
            case '?': usage(argv); break;
	    default: usage(argv); break;
 		}
	} 
      

       /* get optional input file names from trailing 
          parameters and open files */
       if (OptInd < argc) strcpy(Name_Imag_In, argv[OptInd++]);
         else usage(argv);

	if (OptInd < argc) strcpy(Name_Imag_Out, argv[OptInd++]);
         else usage(argv);

	/* make sure there are not too many parameters */
	if (OptInd < argc)
        {
		fprintf(OUTMAN, "Too many parameters: %s ...\n", argv[OptInd]);
		exit(-1);
	}  	
   
#ifdef LARGE_BUFF
    if (OptZ == True) vms_init(VMSSize, VMSName, Verbose);
#endif  
}
 
/*************************************************************************/
    
int main(int argc, char *argv[])
{
    int k;
    fitsstruct Header;
    char Cmd[512];
    Cmd[0] = '\0';
    for (k =0; k < argc; k++) sprintf(Cmd, "%s %s", Cmd, argv[k]);
     
    // Get command line arguments, open input file(s) if necessary
    lm_check(LIC_MR4);
    filtinit(argc, argv);

    if (Verbose == True)
    { 
        cout << endl << endl << "PARAMETERS: " << endl << endl;
        cout << "File Name in = " << Name_Imag_In << endl;
        cout << "File Name Out = " << Name_Imag_Out << endl;  
        // cout << "Ridgelet transform = " <<  StringRidTransform(RidTrans) << endl;  
        if (BlockOverlap == False) cout << "No overlapping " << endl;
	else cout << "Block overlapping " << endl;
        if (BlockSize > 0)  cout << "BlockSize = " << BlockSize <<endl;
        else  cout << "No partitionning  " << endl;
        cout << "Nbr Scale = " <<   NbrScale2D << endl; 
	if (NbrScale1D >= 0)
	 cout << "Nbr Scale 1D = " <<   NbrScale1D << endl;  
	cout << "NSigmaLow = " << NSigmaLow  << endl;
	if (UseSigmaUp == True) cout << "NSigmaUp = " << NSigmaUp  << endl;
	else cout << "MParam = " << MParam  << endl;
	if (QParam != 0) cout << "QParam = " << QParam   << endl;
	if (SParam != 0) cout << "TParam = " << SParam   << endl; 
	if (Clip == True) cout << "Apply a K-sigma clipping: ClipVal = " << ClipVal << endl;
	
    }

    if (NbrScale2D < 1) 
    {
        cout << "Error: number of scales must be larger than 1 ... " << endl;
        exit(-1);
    }
    FilterAnaSynt SelectFilter(F_MALLAT_7_9);
    SubBandFilter SB1D(SelectFilter, NORM_L2);
    Curvelet Cur(SB1D);
    Cur.RidTrans = RidTrans;
    Cur.NbrScale2D = NbrScale2D;
    // NbrScale1D = 2;
    if (NbrScale1D >= 0)
    {
        Cur.NbrScale1D = NbrScale1D;
        Cur.GetAutoNbScale = False;
        if (NbrScale1D < 2) Cur.BlockOverlap = False;
        else Cur.BlockOverlap = BlockOverlap;
    }
    else Cur.BlockOverlap = BlockOverlap;
    Cur.OddBlockSize=True;
    // Cur.Verbose = Verbose;

    Ifloat Data;
    fltarray Trans;
    io_read_ima_float(Name_Imag_In, Data, &Header);
    Header.origin = Cmd;
 
//     if ((BlockSize > 0) && (is_power_of_2(BlockSize) == False))
//     {
//            cout << "Error: Block size must be a power of two ... " << endl;
//            exit(-1);
//     }
    if (BlockSize > Data.nc())
    {
          cout << "Error: Block size must lower than the image size ... " << endl;
          exit(-1);
    }
    Cur.alloc(Data.nl(), Data.nc(), BlockSize);
 
    if (Noise_Ima < FLOAT_EPSILON)
    {
       Noise_Ima = detect_noise_from_bspline (Data);
                   //  detect_noise_from_med (Data);
       if (Verbose == True) cout << "Sigma Noise = " << Noise_Ima << endl;
    }
    CurContrast CT;
    CT.Noise_Ima = Noise_Ima;
    CT.Verbose = Verbose;    
    CT.NSigmaUp = NSigmaUp;
    CT.NSigmaLow = NSigmaLow;
    CT.ClipVal = ClipVal;
    CT.Contrast_P_Param = PParam;
    CT.Contrast_Q_Param = QParam;
    CT.Contrast_M_ParamCoef = MParam;
    CT.L_Coeff = L_Coeff;
    CT.Contrast_S_Param = SParam;
    CT.UseSigmaUp = UseSigmaUp;
    CT.UseVeldeFunction = UseVeldeFunction;
    CT.cur_enhance(Data, Cur, PosIma); 
    // if (UseLog == True) CT.atrou_log(Data, NbrScale2D, True);
    if (Clip == True) CT.sature_clipping(Data);
    io_write_ima_float(Name_Imag_Out, Data, &Header);
    exit(0);
}

;+
; NAME:
;   spcombine
;
; PURPOSE:
;   Calling script for SPCOADD_FRAMES.
;
; CALLING SEQUENCE:
;   spcombine, [ planfile, docams=, adderr=, /xdisplay, minsn2=, /smearinclude ]
;
; INPUTS:
;
; OPTIONAL INPUTS:
;   planfile   - Name(s) of output plan file; default to reducing all
;                plan files matching 'spPlancomb*.par'
;   docams     - Cameras to combine; default to ['b1', 'b2', 'r1', 'r2']
;   adderr     - Additional error to add to the formal errors, as a
;                fraction of the flux; default to 0.03 (3 per cent).
;   xdisplay   - Send plots to X display rather than to plot file
;   minsn2     - Minimum S/N^2 to include science frame in coadd (default 0.2)
;   smearinclude- If set, then include 'smear' flavor exposures as well
;                as 'science' flavor.
;
; OUTPUT:
;
; COMMENTS:
;
; EXAMPLES:
;
; BUGS:
;   This routine spawns the Unix command 'mkdir'.
;
; PROCEDURES CALLED:
;   cpbackup
;   idlspec2d_version()
;   idlutils_version()
;   spcoadd_frames
;   spflux
;   splog
;   yanny_free
;   yanny_par()
;   yanny_read
;
; INTERNAL SUPPORT ROUTINES:
;
; REVISION HISTORY:
;   06-Jul-2000  Written by David Schlegel, Princeton.
;-
;------------------------------------------------------------------------------
pro spcombine, planfile, docams=docams, adderr=adderr, xdisplay=xdisplay, $
 minsn2=minsn2, smearinclude=smearinclude

   if (NOT keyword_set(planfile)) then planfile = findfile('spPlancomb*.par')
   if (n_elements(adderr) EQ 0) then adderr = 0.03
   if (n_elements(minsn2) EQ 0) then minsn2 = 0.2

   ;----------
   ; If multiple plan files exist, then call this script recursively
   ; for each such plan file.

   if (N_elements(planfile) GT 1) then begin
      for i=0, N_elements(planfile)-1 do $
       spcombine, planfile[i], docams=docams, adderr=adderr, $
        xdisplay=xdisplay, minsn=minsn
      return
   endif

   if (NOT keyword_set(docams)) then docams = ['b1', 'b2', 'r1', 'r2']

   ;----------
   ; Strip path from plan file name, and change to that directory

   thisplan = fileandpath(planfile[0], path=thispath)
   cd, thispath, current=origdir
   if (NOT keyword_set(thispath)) then cd, origdir

   ;----------
   ; Find the SPEXP structure

   yanny_read, thisplan, pdata, hdr=hdr
   for i=0, N_elements(pdata)-1 do begin
      if (tag_names(*pdata[i], /structure_name) EQ 'SPEXP') then $
       allseq = *pdata[i]
   endfor
   yanny_free, pdata

   if (N_elements(allseq) EQ 0) then begin
      splog, 'ABORT: No SPEXP structures in plan file ' + thisplan
      cd, origdir
      return
   endif

   ;----------
   ; Find keywords from the header

   extractdir = yanny_par(hdr, 'extractdir')
   combinedir = yanny_par(hdr, 'combinedir')
   logfile = yanny_par(hdr, 'logfile')
   plotfile = yanny_par(hdr, 'plotfile')
   plotsnfile = yanny_par(hdr, 'plotsnfile')
   fcalibprefix = yanny_par(hdr, 'fcalibprefix')
   combinefile = yanny_par(hdr, 'combinefile')
   thismjd = long(yanny_par(hdr, 'MJD'))
   if (NOT keyword_set(thismjd)) then $
    thismjd = max(allseq.mjd)

   if (keyword_set(combinedir)) then $
    spawn, 'mkdir -p ' + combinedir

   stime0 = systime(1)

   ;----------
   ; Open log files for output

   if (keyword_set(logfile)) then begin
      cpbackup, djs_filepath(logfile, root_dir=combinedir)
      splog, filename=djs_filepath(logfile, root_dir=combinedir)
      splog, 'Log file ' + logfile + ' opened ' + systime()
      splog, 'IDL version: ' + string(!version,format='(99(a," "))')
      spawn, 'uname -a', uname
      splog, 'UNAME: ' + uname[0]
   endif
   if (keyword_set(plotfile) AND NOT keyword_set(xdisplay)) then begin
      cpbackup, djs_filepath(plotfile, root_dir=combinedir)
      set_plot, 'ps'
      device, filename=djs_filepath(plotfile, root_dir=combinedir), /color
      splog, 'Plot file ' + plotfile
   endif
   splog, 'Plan file ', thisplan
   splog, 'DOCAMS = ', docams

   splog, 'idlspec2d version ' + idlspec2d_version()
   splog, 'idlutils version ' + idlutils_version()

   camnames = ['b1', 'b2', 'r1', 'r2']
   ncam = N_elements(camnames)

   ;----------
   ; Select frames that match the cameras specified by DOCAM, then trim
   ; to files that aren't names UNKNOWN, and that actually exist on disk.

   for ido=0, n_elements(docams)-1 do begin
      ii = (where(camnames EQ docams[ido], camct))[0]
      if (camct NE 1) then message, 'Non-unique camera ID: ' + docams[ido]
      if (ido EQ 0) then icams = ii $
       else icams = [icams,ii]
   endfor

   objname = allseq.name[icams]

   ; If all file names are UNKNOWN, then abort.
   j = where(objname NE 'UNKNOWN')
   if (j[0] EQ -1) then begin
      splog, 'ABORT: All file names are UNKNOWN in plan file ' + thisplan
      cd, origdir
      return
   endif

   ; Replace all UNKNOWN file names with nulls.
   j = where(objname EQ 'UNKNOWN')
   if (j[0] NE -1) then objname[j] = ''

   ; Replace all file names that do not exist on disk with nulls.
   for ifile=0, n_elements(objname)-1 do $
    if (keyword_set(objname[ifile])) then $
     objname[ifile] = (lookforgzip(djs_filepath(objname[ifile], $
      root_dir=extractdir)))[0]

   ; Now all the file names in ALLSEQ.NAME should exist or be set to null.
   allseq.name[icams] = objname

   j = where(allseq.name[icams])
   if (j[0] EQ -1) then begin
      splog, 'ABORT: No files on disk for plan file ' + thisplan
      cd, origdir
      return
   endif

   ;----------
   ; Compute the spectro-photometry

   objname = (allseq.name[icams])[j]

   spflux, objname, fcalibprefix, outdir=combinedir, adderr=adderr

   ;----------
   ; Close plot file - S/N plots are then put in the PLOTSNFILE file.

   if (keyword_set(plotfile) AND NOT keyword_set(xdisplay)) then begin
      device, /close
      set_plot, 'x'
   endif

   ;----------
   ; Select only the science frames

   if (keyword_set(smearinclude)) then $
    isci = where(allseq.flavor EQ 'science' OR allseq.flavor EQ 'smear') $
   else $
    isci = where(allseq.flavor EQ 'science')

   if (isci[0] EQ -1) then begin 
      splog, 'No science frames in this plan ' + thisplan
      cd, origdir
      return
   endif

   sciname = allseq[isci].name[icams]
   j = where(sciname)

   if j[0] EQ -1 then begin 
      splog, 'No science frames in this plan ' + thisplan
      cd, origdir
      return
   endif

   sciname = sciname[j]

   ;----------
   ;  Check for Minimum S/N in science frame
   ;
   if keyword_set(minsn2) then begin
      nsci = n_elements(sciname)
      framesn2 = fltarr(nsci)

      for i=0,nsci-1 do begin
         checkhdr = headfits(sciname[i])
         if size(checkhdr,/tname) NE 'INT' then begin
           framesn2[i] = sxpar(checkhdr,'FRAMESN2')
           cameras = strtrim(sxpar(checkhdr, 'CAMERAS'),2)
           expstr = string(sxpar(checkhdr, 'EXPOSURE'), format='(i8.8)')
           spectroid = strmid(cameras,1,1)
           corrfile = djs_filepath('spFluxcorr-'+expstr+'-'+spectroid+'.fits', $
                   root_dir=combinedir)
           corrset = mrdfits(corrfile, 1, /silent)
           if size(corrset, /tname) NE 'INT' then begin
             traceset2xy, corrset, tempwave, corrimg
             if total(corrimg) EQ 0 then framesn2[i] = 0.0
           endif else framesn2[i] = 0.0
         endif
      endfor

      j = where(framesn2 GE minsn2)
      if j[0] NE -1 then begin 
        sciname = sciname[j] 
        splog, 'Excluded ', fix(total(framesn2 LT minsn2)), $
             ' frames with SN^2 less than ', minsn2, format='(a,i4,a,f7.2)'
      endif else $
        splog, 'WARNING: All Frames would be rejected due to minimum S/N limit'
   endif

   ;----------
   ; Co-add the fluxed exposures

   spcoadd_frames, sciname, combinefile, mjd=thismjd, combinedir=combinedir, $
    fcalibprefix=fcalibprefix, adderr=adderr, docams=docams, $
    plotsnfile=plotsnfile

   heap_gc   ; garbage collection

   splog, 'Total time for SPCOMBINE = ', systime(1)-stime0, ' seconds', $
    format='(a,f6.0,a)'
   splog, 'Successful completion of SPCOMBINE at ' + systime()

   ;----------
   ; Close log files and change to original directory

   if (keyword_set(logfile)) then splog, /close
   cd, origdir

   return
end
;------------------------------------------------------------------------------

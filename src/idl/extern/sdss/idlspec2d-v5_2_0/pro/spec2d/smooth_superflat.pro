;+
; NAME:
;   smooth_superflat
;
; PURPOSE:
;   Take the superflat fit and target wavelengths and filter superflat
;    to be sure to remove spurious features
;
; CALLING SEQUENCE:
;   smooth_fit = smooth_superflat( superflatset, airset )
;
; INPUTS:
;   superflatset - Superflat bsplineset returned from "superflat"
;   airset       - Wavelength solution (preferably shifted to match skylines)
;
; OPTIONAL KEYWORDS:
;
; OUTPUTS:
;   smooth_fit   - Filtered superflat fit smoothed over about 4 pixels
;
; OPTIONAL OUTPUTS:
;
; COMMENTS:
;
; EXAMPLES:
;
; BUGS:
;
; PROCEDURES CALLED:
;   bspline_iterfit()
;   bspline_valu()
;   djs_oplot
;   djs_plot
;   traceset2xy
;
; REVISION HISTORY:
;   27-Jul-2001  Written by S. Burles, FNAL
;-
;------------------------------------------------------------------------------
function smooth_superflat, superflatset, airset, plottitle=plottitle

   if N_PARAMS() LT 2 then return, 0

   traceset2xy, airset, pixnorm, loglam

   ; sparse sample loglam
   npix    = (size(loglam))[1]
   nfibers = (size(loglam))[2]
   ntotal  = n_elements(loglam)

   ; evaluate at every half-pixel...

   nsparse = npix * 2 + 1
   sparselam = (loglam[sort(loglam)])[lindgen(nsparse)*nfibers/2 < (ntotal-1)]
   model = bspline_valu(sparselam, superflatset)

   ; fit again with fewer breakpoints, one every 4 pixels (8 half pixels)
   ;  and reject with impunity

   invvar = (model > 0) * 2.0e5
   smoothset = bspline_iterfit(sparselam, model, invvar=invvar, $
    lower=3, upper=1.5, everyn=8, maxrej=2, niter=20, $
    groupsize=50, requiren=2, yfit=yfit, outmask=outmask)
 
   bad = where(outmask EQ 0, nbad)

   splog, 'Number of pixels rejected: '+strtrim(string(nbad),2)
   if nbad GT 30 then $
    splog, 'Warning: possible Argon lines in Superflat!'

   ; if pixels were rejected (especially emission lines), then grow one pixel
   ; and refit.

   if nbad GE 1 then begin 
     inmask =  outmask
     inmask[bad - 1 > 0] = 0
     inmask[bad + 1 < (nsparse - 1)] = 0
     smoothset = bspline_iterfit(sparselam, model, invvar=inmask, $
      everyn=8, requiren=2, yfit=yfit, outmask=outmask2)
   endif

   fullfit = bspline_valu(loglam, smoothset)
   ratiodiff = model/(yfit + (yfit LE 0)) - 1.
   ratiodiff = ratiodiff * (yfit GT 0)
   if (keyword_set(inmask)) then ratiodiff = ratiodiff * (inmask NE 0)
   area = total(ratiodiff)/sqrt(nsparse)
   sarea = string(area, format='(f8.4)')

   if total(ratiodiff)/sqrt(nsparse) GT 0.01 then $
    splog, 'WARNING: Possible Argon lines in superflat, flux-fraction=' + sarea
   
   ;----------
   ; Make a QA plot

   if keyword_set(plottitle) then begin
     oldmulti = !p.multi
     !p.multi =[0,1,2]
     wave = 10^sparselam
     xrange = [min(wave),max(wave)] - [1,-1] * (max(wave) - min(wave)) * 0.02

     djs_plot, wave, model, title=plottitle, xchars=0.001,/xs,xrange=xrange
     djs_oplot, wave, yfit, color='red'
     djs_plot, wave, ratiodiff + 1, /ynozero, $
      ymargin=[4,-4], /xstyle, xrange=xrange, xtitle='Wavelength (\AA)'
     djs_oplot, wave, yfit-yfit+1
     xyouts, [0.05], [0.5], 'Fraction of flux in emission lines= '+sarea+' ', $
      alignment=1.0, /normal 
     !p.multi = oldmulti
   endif
 
   return, fullfit
end
;------------------------------------------------------------------------------

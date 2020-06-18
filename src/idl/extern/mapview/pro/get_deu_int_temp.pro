pro get_deu_int_temp, cts, temp
;+
; NAME:
;      GET_DEU_INT_TEMP
;
; PURPOSE:
;      Convert from deu internal temperature counts to temperature in
;      degrees Centrigrad via a lookup table.
;
; CALLING SEQUENCE:
;      get_deu_int_temp, cts, temperature
;
; INPUTS:
;      cts    - fltarr - the raw counts from T/M.  Valid range lies
;                           between 2304 and 3056.
;
; OUTPUTS:
;      temp   - fltarr - temperatures, in degrees Centigrade, which 
;                        correspond to the input counts.
;                        Temperature is set to -1 if an input
;                        count lies outside the valid interpolation
;                        range.
;
; KEYWORDS:
;      none
;
; COMMENTS:
;      Temperatures are linearly interpolated using lookup tables which 
;      are restored from an IDL saveset, deu_internal_temps_lut.xdr, 
;      located in the reference area.  Data contained in the
;      lut saveset were generated by routine make_deu_int_temp_lut.pro,
;      also in the reference area.  Equations were provided by Lil Reichenthal
;      23 Jul 1999, with the same eqn for both DINT1T and DINT2T.
;     
; EXAMPLE:
;      IDL> get_deu_int_temp,2545, temp 
;               temp = 41.8984
; MODIFICATION HISTORY:
;      Original version: J. Weiland,  01 October 1999
;      Use getenv to extract environment variables.  MRG, SSAI, 20 August 2002.
;-
;-------------------------------------------------------------------------
;
fname = concat_dir( strtrim(getenv('MAP_REF'), 2) , $
                    '/prt_data/deu_internal_temps_lut.xdr'
restore,fname
;
; do the linear interpolation
;
temp = interpol(temperature,counts,cts)

too_lo = where(cts lt min(counts), Nlo)
too_hi = where(cts gt max(counts), Nhi)

if (Nlo GT 0) then temp[too_lo] = -1
if (Nhi GT 0) then temp[too_hi] = -1

;
;
return
end


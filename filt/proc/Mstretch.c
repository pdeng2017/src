/* Stretch of the time axis. */
/*
  Copyright (C) 2004 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <math.h>
#include <string.h>
#include <float.h>

#include <rsf.h>

#include "fint1.h"

static float t0, x0, h=0.;

static float t2(float t) { return t*t; }
static float log_frw(float t) { return logf(t/t0); }
static float log_inv(float t) { return t0*expf(t); }
static float nmo(float t) 
{ 
    t = t*t + h;
    return (t > 0.)? sqrtf(t): 0.; 
} 
static float lmo(float t) { return t + h; } 
static float rad_inv(float t) { return (t-t0)*(h+x0)/x0; }
static float rad_frw(float t) { return t0-t*x0/(h-x0); }

int main(int argc, char* argv[])
{
    fint1 str;
    bool inv, half;
    int i2, n1,n2, i3, n3, n, dens, nw, CDPtype;
    float d1, o1, d2, o2, *trace, *stretched, h0, dh, v0, d3;
    float (*forward)(float) = NULL, (*inverse)(float) = NULL;
    char *rule, *prog;
    sf_file in, out;

    sf_init (argc,argv);
    in = sf_input("in");
    out = sf_output("out");

    if (!sf_histint(in,"n1",&n1)) sf_error("No n1= in input");
    if (!sf_histint(in,"n2",&n2)) n2=1;
    n3 = sf_leftsize(in,2);

    if (!sf_getbool("inv",&inv)) inv=false;
    /* if y, do inverse stretching */
    if (!sf_getint("dens",&dens)) dens=1;
    /* axis stretching factor */

    if (!sf_histfloat(in,"o1",&o1)) sf_error("No o1= in input");
    if (!sf_histfloat(in,"d1",&d1)) sf_error("No d1= in input");

    prog = sf_getprog();
    if (NULL != strstr (prog, "log")) {
	rule="Log";
    } else if (NULL != strstr (prog, "t2")) {
	rule="2";
    } else if (NULL != strstr (prog, "lmo")) {
	rule="lmo";
    } else if (NULL != strstr (prog,"nmo") || 
	       NULL == (rule = sf_getstring("rule"))) {
	/* Stretch rule:
	   n - normal moveout
	   l - linear moveout
	   L - logstretch
	   2 - t^2 stretch
	   r - radial moveout
	*/
	rule="nmo";
    }

    if ('n'==rule[0] || 'l'==rule[0] || 'r'==rule[0]) {
	if (!sf_histfloat(in,"o2",&h0)) sf_error("No o2= in input"); 
	if (!sf_histfloat(in,"d2",&dh)) sf_error("No d2= in input"); 
	if (!sf_getfloat("v0",&v0)) sf_error("Need v0=");
	/* moveout velocity */
	sf_putfloat(out,"v0",v0);
	
	if (!sf_getbool("half",&half)) half=true;
	/* if y, the second axis is half-offset instead of full offset */
	if (half) {
	    dh *= 2.;
	    h0 *= 2.;
	}
	
	CDPtype=1;
	if (sf_histfloat(in,"d3",&d3)) {
	    CDPtype=0.5+0.5*dh/d3;
	    if (1 != CDPtype) sf_histint(in,"CDPtype",&CDPtype);
	    if (1 > CDPtype) CDPtype=1;
	} 	    
	sf_warning("CDPtype=%d",CDPtype);
	
	h0 /= v0; 
	dh /= v0;
    }

    switch (rule[0]) {
	case 'n':
	    forward = inverse = nmo;
	    break;
	case 'l':
	    forward = inverse = lmo;
	    if (!sf_getfloat("delay",&t0)) sf_error("Need delay=");
	    /* time delay for rule=lmo */
	    h = -t0;

	    break;
	case 'L':
	    forward = log_frw;
	    inverse = log_inv;

	    if (o1 < FLT_EPSILON) o1=FLT_EPSILON;
	    if (!sf_getfloat ("t1", &t0) && 
		!sf_histfloat(in,"t1",&t0)) t0=o1;
	    sf_putfloat(out,"t1",t0);
	    break;
	case '2':
	    forward = t2;
	    inverse = sqrtf;
	    
	    if (o1 < FLT_EPSILON) o1=FLT_EPSILON;
	    break;
	case 'r':
	    if (!sf_getfloat("tdelay",&t0)) sf_error("Need tdelay=");
	    /* time delay for rule=rad */
	    if (!sf_getfloat("hdelay",&x0)) sf_error("Need hdelay=");
	    /* offset delay for rule=rad */

	    forward = rad_frw;
	    inverse = rad_inv;
	    break;
	default:
	    sf_error("rule=%s is not implemented",rule);
	    break;
    }

    if (inv) {
	if (!sf_histint(in,"nin",&n)) n=n1/dens;

	o2 = inverse(o1);
	d2 = (inverse(o1+(n1-1)*d1) - o2)/(n-1);
    } else {
	if (!sf_getint("nout",&n)) n=dens*n1;
	/* output axis length (if inv=n) */
	sf_putint(out,"nin",n1);

	o2 = forward(o1);
	d2 = o1+(n1-1)*d1;
	d2 = (forward(d2) - o2)/(n-1);
    }

    sf_putint  (out,"n1",n);
    sf_putfloat(out,"o1",o2);
    sf_putfloat(out,"d1",d2);

    trace = sf_floatalloc(n1);
    stretched = sf_floatalloc(n);

    if (!sf_getint("extend",&nw)) nw=4;
    /* trace extension */
    str = fint1_init(nw,n1);

    for (i3=0; i3 < n3; i3++) {
	for (i2=0; i2 < n2; i2++) {
	    if ('l' == rule[0] || 'n' == rule[0] || 'r' == rule[0]) {
		h = h0+i2*dh + (dh/CDPtype)*(i3%CDPtype);
		if ('n' == rule[0]) h *= h;
		if ('l' == rule[0]) h = fabsf(h);
		if (inv) h = -h;
	    } 

	    sf_floatread (trace,n1,in);
	    fint1_set(str,trace);
	    
	    stretch(str,inv? forward: inverse, 
		    n1, d1, o1, n, d2, o2, stretched);

	    sf_floatwrite (stretched,n,out);
	}
    }

    exit (0);
}

/* 	$Id$	 */

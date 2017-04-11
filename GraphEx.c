#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

double opacity = 0.5;
int axisfree = 0; 
struct colordata{
GdkColor color;
int numpixels;
int *x;
int *y;
};
cairo_surface_t *graphsurface;
int xlow = -1, xhigh = 10000, ylow = -1, yhigh = 10000;

int ylowColorBar = -1,xlowColorBar = -1, xhighColorBar = 10000, yhighColorBar = 10000;
int colorbar = 0;

#define maxwindowcolors 7500
int numbits = 24;//Num bits to use, reducing this in 3 bit increments can greatly reduce the workload

//#define maxclustersize 10000
struct cluster
{
	int colors[maxwindowcolors];
	int numcolors;
	int numpixels;
	int left, top, width,height;
	int draw;
};

struct colordata windowcolors[maxwindowcolors];
int numwindowcolors;


//int colordisplaymax = 50;
GtkWidget *colorwindow;
GtkWidget *colorwindowvbox;
GtkWidget *colorarea;
int globalpadding = 4;

GtkWidget *grapharea;
GtkWidget *GraphWindow;


GtkWidget *xentrylow;
GtkWidget *xentryhigh;
GtkWidget *yentrylow;
GtkWidget *yentryhigh;
GtkWidget *colorbarlow;
GtkWidget *colorbarhigh;
//int totalpixels;



#define maxclusters maxwindowcolors
struct cluster clusters[maxclusters];
int numclusters;
double *distancematrix;

int colorbarclusters[maxclusters];
double colorbarclustervalues[maxclusters];
int numcolorbarclusters = 0;


int previoustoggle = 0;


int *drawingarray;


int transparent = 0;

double clusteringinitializationvariable = .025;
//double clusteringcontinuationvariable = 2.25;

FILE *clusterplotpipe;
GtkWidget *sensitivity;


double *x;//The final form of the data
double *y;
double *z;
int numpoints=0;

int colorareawidth,colorareaheight;

double rgbcolordistance(GdkColor color1, GdkColor color2)
{
	double value;
	value = pow((double)color1.red/65536 - (double)color2.red/65536,2) + pow((double)color1.green/65536 - (double)color2.green/65536,2) + pow((double)color1.blue/65536 - (double)color2.blue/65536,2);
	value = sqrt(value);
	//printf("Color 1 %d %d %d Color 2 %d %d %d Distance %g\n",color1.red,color1.green,color1.blue,color2.red,color2.green,color2.blue,value);
	return value;
}

double hsvcolordistance(GdkColor color1, GdkColor color2)
{
	double value;
	double h1,s1,v1,h2,s2,v2;
	gtk_rgb_to_hsv((double)color1.red/65536,(double)color1.green/65536,(double)color1.blue/65536,&h1,&s1,&v1);
	gtk_rgb_to_hsv((double)color2.red/65536,(double)color2.green/65536,(double)color2.blue/65536,&h2,&s2,&v2);
	value = pow(h1 - h2,2) + pow(s1 - s2,2) + pow(v1 - v2,2);
	value = sqrt(value);
	//printf("Color 1 %d %d %d Color 2 %d %d %d Distance %g\n",color1.red,color1.green,color1.blue,color2.red,color2.green,color2.blue,value);
	return value;
}

double colordistance(GdkColor color1, GdkColor color2)
{
	//Treats colors as a vector (r,g,b) and finds distance between two
	
	double value;
	//value = fmin(rgbcolordistance(color1,color2),hsvcolordistance(color1,color2));
	//value = sqrt(value);
	value = rgbcolordistance(color1,color2);
	//printf("Color 1 %d %d %d Color 2 %d %d %d Distance %g\n",color1.red,color1.green,color1.blue,color2.red,color2.green,color2.blue,value);
	return value;
}



void clustercallback()
{
	clusterdetection();
	printf("Sorting clusters by size\n");
	bubblesort(0,1,3,NULL);
	printf("Packing rectangles\n");
	//drawcolorwindow();
	packrectangles(globalpadding);
	gtk_widget_set_size_request(colorarea,colorareawidth,colorareaheight);
	//drawcolorarea();
	gtk_widget_queue_draw(colorarea);
	
	/*
	int numclusterpixels = 0;
	int index;
	for(index = 0;index<numclusters;index++)
	numclusterpixels += (clusters+index)->numpixels;
	printf("Num pixels %d num cluster pixels %d\n",totalpixels,numclusterpixels);
*/
}

void readcolorsfromwindow(GtkWidget *window)
{
	gint width,height;
	//gtk_window_get_size(window,&width,&height);
	
	//gtk_window_get_position(window,&xoffset,&yoffset);
	
	GtkAllocation *allocation = (GtkAllocation *)malloc(sizeof(GtkAllocation));		//Create the drawable pixmap
	gtk_widget_get_allocation(grapharea, allocation);
	width = allocation->width;
	height = allocation->height;
	
	//GtkAllocation *allocation = (GtkAllocation *)malloc(sizeof(GtkAllocation));		//Create the drawable pixmap
	
	
	
	//gtk_widget_get_size(window,&width,&height);
	//gtk_widget_get_position(window,&xoffset,&yoffset);
	


	gint wx,wy;
	int xoffset,yoffset;
	gtk_widget_translate_coordinates(grapharea, GraphWindow, 0, 0, &xoffset, &yoffset);
	gdk_window_get_origin (gtk_widget_get_window (grapharea), &wx, &wy);
	GdkWindow *root_window = gdk_get_default_root_window ();
	
	//GtkAllocation *allocation2 = (GtkAllocation *)malloc(sizeof(GtkAllocation));
	//gint rootwidth,rootheight;
	//gtk_widget_get_allocation(root_window, allocation2);
	//rootwidth = allocation2->width;
	//rootheight = allocation2->height;
	printf("Dimensions of drawing area %d x %d\n",gtk_widget_get_allocated_width (grapharea),gtk_widget_get_allocated_height (grapharea));
	printf("Offset of window %d x %d offset of area from window %d x %d size of area %d x %d\n",wx,wy,xoffset,yoffset,width,height);
	//wx+= xoffset;
	//wy+= yoffset;
	GdkPixbuf *pixbuf =	gdk_pixbuf_get_from_window (root_window,wx,wy,width,height);
	//GdkPixbuf *pixbuf =	gdk_pixbuf_get_from_window (gtk_widget_get_window (grapharea),xoffset,yoffset,width,height);
	
	printf("%d pixels\n",width*height);
	printf("Pixbuf %d x %d rowstride %d Channels %d Bits/sample %d\n",gdk_pixbuf_get_width(pixbuf),gdk_pixbuf_get_height(pixbuf),gdk_pixbuf_get_rowstride(pixbuf),gdk_pixbuf_get_n_channels(pixbuf),gdk_pixbuf_get_bits_per_sample(pixbuf));
	const guint8 *pixel = gdk_pixbuf_get_pixels(pixbuf);

	GdkColor color;
	
	int x,y;
	int offset;
	int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	int numchannels = 3;//rgb
	//int numchannels = 4;//rgb
	int windowcolorindex;
	
	for(;numwindowcolors>0;numwindowcolors--)
	{
		free((windowcolors+numwindowcolors-1)->x);
		free((windowcolors+numwindowcolors-1)->y);
	}
	
	
	double h1, s1, v1;
	double h2, s2, v2;
	int black, white;
	
	struct colordata *windowcolor;
	numwindowcolors = maxwindowcolors;
	
	numbits = 27;
	int bitsperchannel;
	
	while(numwindowcolors == maxwindowcolors)
	{
	numbits -=3;
	if(numbits <24)
	printf("Reducing num bits to %d to improve speed\n",numbits);
	
	bitsperchannel = numbits/3;
	numwindowcolors = 0;
	for(x=0;x<width;x++)
	for(y=0;y<height && numwindowcolors<maxwindowcolors;y++)
	{
	//offset = numchannels *(x+y*width);
	offset = numchannels *x + y* rowstride;
	//offset = numchannels *(y+x*height);
	color.red = *(pixel+offset) * pow(2,bitsperchannel-8);//Truncation occurs after first multiplication, this truncation helps to reduce the total number of unique colors
	color.green =*(pixel+offset+1) * pow(2,bitsperchannel-8);
	color.blue = *(pixel+offset+2) * pow(2,bitsperchannel-8);
	color.red *= pow(2,16-bitsperchannel);
	color.green *= pow(2,16-bitsperchannel);
	color.blue *=  pow(2,16-bitsperchannel);
	
	
	gtk_rgb_to_hsv((double)color.red/256/256,(double)color.green/256/256,(double)color.blue/256/256,&h1,&s1,&v1);
	
		for(windowcolorindex=0;windowcolorindex<numwindowcolors;windowcolorindex++)
		{
		windowcolor = (windowcolors+windowcolorindex);
		gtk_rgb_to_hsv((double)windowcolor->color.red/256/256,(double)windowcolor->color.green/256/256,(double)windowcolor->color.blue/256/256,&h2,&s2,&v2);
		white = s1 == 0 && s2 == 0;
		black = v1 == 0 && v2 == 0;
		if(black || white || windowcolor->color.red == color.red && windowcolor->color.green == color.green && windowcolor->color.blue == color.blue)
		{
			*(windowcolor->x+windowcolor->numpixels)=x;
			*(windowcolor->y+windowcolor->numpixels)=y;
			(windowcolor->numpixels)++;
			break;
		}
		}
		if(windowcolorindex == numwindowcolors)//Unable to find color in list, add new entry to list
		{
			*(drawingarray+numwindowcolors) = 0;
			(windowcolors+numwindowcolors)->color = color;
			(windowcolors+numwindowcolors)->x = malloc(width*height*sizeof(int));
			(windowcolors+numwindowcolors)->y = malloc(width*height*sizeof(int));
			*((windowcolors+numwindowcolors)->x) = x;
			*((windowcolors+numwindowcolors)->y) = y;
			((windowcolors+numwindowcolors)->numpixels) = 1;
			numwindowcolors++;
		}
	}
	}
	//bubblesort(0,0,3,NULL);
	//printcolors();
	//printcolordistance(0);
	printf("Geometry %d x %d + %d x %d Num window colors %d Numbits %d\n",width,height,wx,wy,numwindowcolors,numbits);

	
	filldistancematrix();
	clustercallback();
	
	
	//totalpixels = width*height;
	gtk_widget_show_all(colorwindow);
}

void colorbarclustering()
{
	if(colorbar == 0)
	return;
	printf("Starting colorbar clustering\n");
	
	double v1=0,v2=0;
	char *buffer = gtk_entry_get_text(colorbarlow);
	//printf("%s %d %g\n",buffer,strlen(buffer),atof(buffer));
	if(strlen(buffer) > 0)
	v1 = atof(buffer);
	buffer = gtk_entry_get_text(colorbarhigh);
	//printf("%s %d %g\n",buffer,strlen(buffer),atof(buffer));
	if(strlen(buffer) > 0)
	v2 = atof(buffer);
	
	
	
	int x,y;
	int distance = pow(pow(xlowColorBar-xhighColorBar,2) + pow(ylowColorBar-yhighColorBar,2),0.5);
	double value;
	int index;
	int wc;
	int coordinate;
	numcolorbarclusters = 0;
	int c;
	int subcolor;
	int ccheck;
	for(index = 0;index<=distance;index++)
	{
		x = xlowColorBar + (double)index/distance * (xhighColorBar-xlowColorBar);
		y = ylowColorBar + (double)index/distance * (yhighColorBar-ylowColorBar);
		value = v1 + (double)index/distance*(v2-v1);
		for(wc = 0;wc<numwindowcolors;wc++)
		{
			for(coordinate=0;coordinate<(windowcolors+wc)->numpixels;coordinate++)
			{
				if(*((windowcolors+wc)->x+coordinate) == x && *((windowcolors+wc)->y+coordinate) == y)
				break;
			}
			if(coordinate <(windowcolors+wc)->numpixels)
				break;
		}
		if(wc >= numwindowcolors)
		printf("The correct color was not found\n");
		for(c=0;c<numclusters;c++)
		{
			for(subcolor=0;subcolor<(clusters+c)->numcolors;subcolor++)
			{
				if(wc == *((clusters+c)->colors+subcolor))//correct cluster has been identified, add to list if it is not already there
				{
					for(ccheck=0;ccheck<numcolorbarclusters;ccheck++)
					{
					if(c == *(colorbarclusters+ccheck))
					break;
					}
					if(ccheck >= numcolorbarclusters)
					{
						*(colorbarclusters + numcolorbarclusters) = c;
						*(colorbarclustervalues + numcolorbarclusters) = value;
						numcolorbarclusters++;
					}
					break;
				}
			}
			if(subcolor<(clusters+c)->numcolors)
			break;
		}
	}
	printf("Finished colorbar clustering\n");

}



void toggletransparency(GtkWidget *window)
{
	//if(gtk_window_get_opacity(window) == 0)
	//gtk_window_set_opacity(window,opacity);
	//else
	//gtk_window_set_opacity(window,0);
	transparent = !transparent;
	gtk_widget_queue_draw(GraphWindow);
}

void printcolors()
{
	int index;
	 
	for(index = 0;index<numwindowcolors;index++)
	{	
		printf("Numpixels %d ",(windowcolors+index)->numpixels);
		printcolor((windowcolors+index)->color);
	}
}

void printcolor(GdkColor color)
{
	double h,s,v;

	gtk_rgb_to_hsv((double)color.red/256/256,(double)color.green/256/256,(double)color.blue/256/256,&h,&s,&v);
	printf("R %d G %d B %d H %d S %d V %d\n",color.red,color.green,color.blue,(int)(h*256*256),(int)(s*256*256),(int)(v*256*256));
}

void drawcluster(cairo_t *cr,struct cluster *c)

{
	
		//printf("drawing area %d x %d + %d x %d, %d pixels\n",c->left,c->top,c->width,c->height,c->numpixels);

	/*
	GtkAllocation *allocation = (GtkAllocation *)malloc(sizeof(GtkAllocation));		//Create the drawable pixmap
	gtk_widget_get_allocation(drawingarea, allocation);
	guint width = allocation->width;
	guint height = allocation->height;
	//printf("dimensions %d x %d\n",width,height);
	
	cairo_t *cr;
	cairo_surface_t *surface;
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width, height);
	cr = cairo_create (surface);
	
	*/
	if(c->draw)
	{
		//printf("Drawing box around cluster\n");
		cairo_set_source_rgb (cr, 1.0f, (double)165/256, 0);
		cairo_set_line_width(cr,globalpadding/2);
		cairo_rectangle(cr,c->left-globalpadding/2,c->top-globalpadding/2,c->width+globalpadding,c->height+globalpadding);
		cairo_stroke(cr);
	}
	
	
	GdkColor color;
	int x = c->left,y = c->top;
	int colorindex,pixelindex;
	struct colordata *cd;
	
	for(colorindex = 0;colorindex<c->numcolors;colorindex++)
	{
	cd = (windowcolors+*(c->colors+colorindex));
	color = cd->color;
	cairo_set_source_rgb (cr, (double)color.red/65536, (double)color.green/65536, (double)color.blue/65536);
	for(pixelindex = 0;pixelindex<cd->numpixels;pixelindex++)
	{
	cairo_rectangle(cr,x,y,1,1);
	cairo_fill(cr);
	if(x == c->left+c->width -1)//next row
	{
		y++;
		x = c->left;
	}
	else
	{
		x++;
	}
	}
	}
	
	
	//cairo_destroy(surface);
}
void deletecolorbar()
{
	colorbar = 0;
}

void drawcolorarea()
{
	//printf("Drawing color area\n");
	GtkAllocation *allocation = (GtkAllocation *)malloc(sizeof(GtkAllocation));		//Create the drawable pixmap
	gtk_widget_get_allocation(colorarea, allocation);
	guint width = allocation->width;
	guint height = allocation->height;
	printf("Drawing Color Area WIth size %d x %d\n",width,height);
	
	cairo_t *cr;
	
	cairo_surface_t *colorsurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width, height);
	cr = cairo_create (colorsurface);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle(cr,0,0,width,height);
	cairo_fill(cr);
	
	int index;
	for(index = 0;index<numclusters;index++)
	{
		//printf("Drawing cluster %d\n",index);
		drawcluster(cr,clusters+index);
	}
	
	cairo_t *cr2;	
	cr2 = gdk_cairo_create (gtk_widget_get_window(colorarea));
	cairo_set_source_surface(cr2, colorsurface, 0, 0);
	cairo_paint(cr2);
	
	cairo_destroy(cr2);
	cairo_destroy(cr);
}

void bubblesort(int ascending, int useclusters,int sorttype, GdkColor color)
{
	//sort types 0 h, 1 s, 2 v, 3 numpixels, 4 distance to color
	int index1,index2;
	struct colordata swapdata;
	double  value1, value2;
	double h1, s1, v1;
	double h2, s2, v2;
	struct colordata *data1,*data2;
	struct cluster *c1,*c2,clusterswap;
	
	/*if(sorttype == 4)
	{
	printf("sorting to color \n");
	printcolor(color);
	}*/
	int drawswap;
	int max;
	
	if(useclusters)
	max = numclusters;
	else
	max = numwindowcolors;
	//printf("Max is %d\n",max);
	for(index1 = 0;index1<max;index1++)
	{
		//printf("Index %d\n",index1);
		for(index2 = 0;index2<max-index1-1;index2++)
		{	
			
			if(!ascending)
			{
			if(useclusters)
			{
				
				c1 = (clusters+index2);
				c2 = (clusters+index2+1);
				//printf("Clustering and not ascending\n");
				//printf(" %d and %d colors\n",c1->numcolors,c2->numcolors);
				//printf("Colors are %d and %d\n",*(c1->colors),*(c2->colors));
				data1 = windowcolors + *(c1->colors);
				data2 = windowcolors + *(c2->colors);
				
			}
			else
			{
			data1 = (windowcolors+index2);
			data2 = (windowcolors+index2+1);
			}
			}
			else
			{
				
			if(useclusters)
			{
				//printf("Clustering and ascending\n");
				c1 = (clusters+max-2-index2);
				c2 = (clusters+max-index2-1);
				//printf("Colors are %d and %d\n",*(c1->colors),*(c2->colors));
				data1 = windowcolors + *(c1->colors);
				data2 = windowcolors + *(c2->colors);
			}
			else
			{
			data1 = (windowcolors+numwindowcolors-2-index2);
			data2 = (windowcolors+numwindowcolors-index2-1);	
			}
			}	
			//printf("Read data in\n");
			gtk_rgb_to_hsv((double)data1->color.red/256/256,(double)data1->color.green/256/256,(double)data1->color.blue/256/256,&h1,&s1,&v1);
			gtk_rgb_to_hsv((double)data2->color.red/256/256,(double)data2->color.green/256/256,(double)data2->color.blue/256/256,&h2,&s2,&v2);
			//printf("Converted to hsv\n");
			switch(sorttype)
			{
				case 0:
				value1 = h1;
				value2 = h2;
				break;
				
				case 1:
				value1 = s1;
				value2 = s2;
				break;
				
				case 2:
				value1 = v1;
				value2 = v2;
				break;
				
				case 3:
				if(useclusters)
				{
				value1 = c1->numpixels;
				value2 = c2->numpixels;
				}
				else
				{
				value1 = data1->numpixels;
				value2 = data2->numpixels;
				}
				break;
				
				case 4:
				value1 = colordistance(color,data1->color);
				value2 = colordistance(color,data2->color);
				break;
				
				default:
				value1 = 0;
				value2 = 0;
				break;
			}
			if(!ascending && value1 < value2)
			{
				//printf("Swapping\n");
				if(useclusters)
				{
				clusterswap = *c1;
				*c1 = *c2;
				*c2 = clusterswap;
					}
				else
				{
				swapdata = *(windowcolors+index2);
				*(windowcolors+index2) = *(windowcolors+index2+1);
				*(windowcolors+index2+1) = swapdata;
				
				drawswap = *(drawingarray+index2);
				*(drawingarray+index2) = *(drawingarray+index2+1);
				*(drawingarray+index2+1) = drawswap; 
				}
			}
			if(ascending && value1 > value2)
			{
				if(useclusters)
				{
				clusterswap = *c1;
				*c1 = *c2;
				*c2 = clusterswap;
					}
				else
				{
				swapdata = *(windowcolors+numwindowcolors-2-index2);
				*(windowcolors+numwindowcolors-2-index2) = *(windowcolors+numwindowcolors-index2-1);
				*(windowcolors+numwindowcolors-index2-1) = swapdata;
				
				drawswap = *(drawingarray+numwindowcolors-2-index2);
				*(drawingarray+numwindowcolors-2-index2) = *(drawingarray+numwindowcolors-index2-1);
				*(drawingarray+numwindowcolors-index2-1) = drawswap; 
				}
			}
		}
	}
	/*
	if(sorttype == 4)
	{
	printf("sorted to color: \n");
	printcolor(color);
	}*/
}

void toggledraw(GtkWidget *checkbox, int clusterindex)
{
	int toggled = gtk_toggle_button_get_active(checkbox);
	printf("Changed index %d to %d\n",clusterindex,toggled);
	
	//printcolor((windowcolors+windowcolorindex)->color);
	
	//bubblesort(1,4,(windowcolors+windowcolorindex)->color);
	//printcolordistance(0);
	plotclusters((windowcolors+*((clusters+clusterindex)->colors))->color);
	//int clustersize = clusteralgorithm();
	//int clusterindex,subindex;
	
	if(clusterindex == numclusters)
	{
		printf("Cluster not found \n");
		return;
	}
	struct cluster *c = clusters+clusterindex;
	
	int colorindex;
	for(colorindex=0;colorindex<c->numcolors;colorindex++)
	{
	*(drawingarray+*(c->colors+colorindex)) = toggled;
	//printf("Setting color %d with %d pixels to %d Distance to main %g\n",*(c->colors+colorindex),(windowcolors+*(c->colors+colorindex))->numpixels, toggled,colordistance((windowcolors+ *(c->colors))->color,(windowcolors+*(c->colors+colorindex))->color));
	}
	//drawgrapharea();
	gtk_widget_queue_draw (grapharea);
	
	//printcolor((windowcolors+windowcolorindex)->color);
	
	//GdkColor null;
	//bubblesort(0,3,null);

	//drawcolorwindow();
	
	printf("Distance from color %d to color %d %g\n",clusterindex,previoustoggle,colordistance((windowcolors+ *(c->colors))->color,(windowcolors+*(clusters+previoustoggle)->colors)->color));
	previoustoggle = clusterindex;
}

int contains(int *list, int search, int numitems)
{
	if(numitems == 0)
	return 0;
	
	int index;
	for(index = 0;index<numitems;index++)
	if(*(list+index) == search)
	return 1;
	
	return 0;
}

void filldistancematrix()
{
	printf("Filling distance matrix\n");
	distancematrix = malloc(numwindowcolors*numwindowcolors*sizeof(double));
	int index1,index2;
	for(index1=0;index1<numwindowcolors;index1++)
	for(index2=index1+1;index2<numwindowcolors;index2++)
	{
		*(distancematrix + index1*numwindowcolors+index2) = colordistance((windowcolors+index1)->color,(windowcolors+index2)->color);
		*(distancematrix + index2*numwindowcolors+index1) = *(distancematrix + index1*numwindowcolors+index2);
	}
	printf("Done Filling distance matrix\n");
}


void packrectangles(int padding)
{
	printf("Padding %d\n",padding);
	//Attempts to pack the clusters into a rectangular area;
	gint width,height;
	GtkAllocation *allocation = (GtkAllocation *)malloc(sizeof(GtkAllocation));		//Create the drawable pixmap
	gtk_widget_get_allocation(grapharea, allocation);
	width = allocation->width;
	height = allocation->height;
	
	int maxwidth = width * 1.5;//The furthest extent of any placed rectangle
	int maxheight = height;
	
	int numstacks = 1;
	int stackindex = 0;
	
//	int currenty = padding;
	int rowy = padding;
	int rowx = padding;//Location of next placement when stack is full
	int lasty=rowy;
//	int currentx = padding;
	int lastx=rowx;
	
	int clusterindex = 0;
	int heightzero;
	
	for(clusterindex=0;clusterindex<numclusters;clusterindex++)
	{
		if(clusterindex == 0)
		{
			height = 1+padding;
			while(height < 0.75*sqrt(clusters->numpixels))
			height = (height+padding)*2;
			width = ceil(clusters->numpixels/height);
			heightzero = height;
		}
		else
		{
			//height = sqrt(clusters->numpixels)/numstacks - (numstacks-1)*padding;
			width = ceil((clusters+clusterindex)->numpixels/(double)height);
			while(width < height/2 && height > 1+padding)
			{
				//printf("Dimensions %d x %d too thin\n",width,height);
				stackindex *= 2;
				numstacks *= 2;
				//height = sqrt(clusters->numpixels)/numstacks - (numstacks-1)*padding;
				height = height/2 - padding;
				width = ceil((clusters+clusterindex)->numpixels/(double)height);
			}
			if(height < 1)
			printf("Height < 1 height %d numstacks %d\n",height,numstacks);
		}
		//printf("Cluster %d numpixels %d height %d width %d numstacks %d stackindex %d\n",clusterindex,(clusters+clusterindex)->numpixels,height,width,numstacks,stackindex);
		
		
		//nextx = currentx+width+padding;
		
		if(stackindex == 0)
		{
			if(rowx+width > maxwidth)
			{
				rowx = padding;
				rowy += heightzero+padding;
			}
			(clusters+clusterindex)->left = rowx;
			(clusters+clusterindex)->top = rowy;
			rowx += width + padding;
		}
		else
		{
				(clusters+clusterindex)->left = lastx;
				(clusters+clusterindex)->top = lasty+padding;
		}
		
		
		//(clusters+clusterindex)->left = nextx;
		(clusters+clusterindex)->width = width;
		//(clusters+clusterindex)->top = nexty;
		(clusters+clusterindex)->height = height;
		
		
		if((clusters+clusterindex)->left + width > rowx)
		rowx = (clusters+clusterindex)->left + width;
		
		//printf("Cluster %d top left %d x %d width %d height %d stackindex %d numstacks %d\n last %d x %d row %d x %d\n",clusterindex,(clusters+clusterindex)->left,(clusters+clusterindex)->top,width,height,stackindex,numstacks,lastx,lasty,rowx,rowy);
	
		lastx = (clusters+clusterindex)->left;
		lasty = (clusters+clusterindex)->top+height;
		
		if(lasty+padding > maxheight)
		maxheight = lasty+padding;
		
		if(lastx+width+padding > maxwidth)//expand maxwidth
		maxwidth = rowx + width+padding;
		
		
		stackindex++;
		if(stackindex == numstacks)
		stackindex = 0;
		
		/*
		if(nextx + width + padding > rowx)//Move location of next column
		rowx = nextx + width + padding;
		
		if(stackindex == 0 && rowx + width > maxwidth)//place in next row
		{
			rowy += padding + heightzero;
			rowx = padding;
		}
		
		
		
		if(stackindex == 0)//place in next column/row
		{
			nextx =  rowx;
			nexty = rowy;
		}
		else//place lower in stack
		nexty += nexty+height + padding;
		*/	
		
		
		

	}
	colorareawidth = maxwidth;
	colorareaheight = maxheight;
	
	
	
}

void clusterdetection()
{
	
	clusteringinitializationvariable = gtk_range_get_value(sensitivity);
	cleardrawarray();
	
	printf("Starting Cluster Detection with %d colors and %g sensistivity\n",numwindowcolors,clusteringinitializationvariable);
	numclusters = 0;
	double distance;
	
	int *adjacencylist = malloc(numwindowcolors*numwindowcolors*sizeof(int));
	int *numadjacent = malloc(numwindowcolors*sizeof(int));
	
	
	int index1,index2;
	printf("Filling adjacency list\n");
	for(index1=0;index1<numwindowcolors;index1++)
	numadjacent[index1] = 0;
	for(index1=0;index1<numwindowcolors;index1++)
	for(index2=index1+1;index2<numwindowcolors;index2++)
	{
		//distance = colordistance((windowcolors+index1)->color,(windowcolors+index2)->color);
		if(*(distancematrix+index1*numwindowcolors+index2) < clusteringinitializationvariable)
		{
			*(adjacencylist+index1*numwindowcolors + *(numadjacent+index1)) = index2;
			*(adjacencylist+index2*numwindowcolors + *(numadjacent+index2)) = index1;
			(*(numadjacent+index1))++;
			(*(numadjacent+index2))++;
		}
	}	printf("Done Filling adjacency list\n");
		
	int subcolorindex,subsubcolorindex;	
	int usedcolors[numwindowcolors];
	int numusedcolors = 0;
	int colorindex,clustercolorindex,adjacencyindex;
	for(colorindex=0;colorindex<numwindowcolors;colorindex++)//Iteratre over colors
		if(!contains(usedcolors,colorindex,numusedcolors))//must have at least 1 neighbor and not have been used before
		{
			*(usedcolors+numusedcolors) = colorindex;
			numusedcolors++; 
			
			*((clusters+numclusters)->colors + ((clusters+numclusters)->numcolors)) = colorindex;
			//printf("Assigning color %d to cluster %d\n",colorindex,numclusters);
			//printcolor((windowcolors+colorindex)->color);
			(clusters+numclusters)->numcolors = 1;	
			(clusters+numclusters)->numpixels = (windowcolors+colorindex)->numpixels;
		for(clustercolorindex=0;clustercolorindex<(clusters+numclusters)->numcolors;clustercolorindex++)//Iterate over colors already in cluster
		{
			subcolorindex = *((clusters+numclusters)->colors + clustercolorindex);
		for(adjacencyindex=0;adjacencyindex<*(numadjacent+subcolorindex);adjacencyindex++)//Iterate over adjacency list of colors in cluster
		{
			subsubcolorindex = *(adjacencylist + subcolorindex*numwindowcolors + adjacencyindex);
			if(!contains(usedcolors,subsubcolorindex,numusedcolors))
			{
				//printf("Assigning color %d to cluster %d because of color %d\n",subsubcolorindex,numclusters,subcolorindex);
				*((clusters+numclusters)->colors + ((clusters+numclusters)->numcolors)) = subsubcolorindex;
				((clusters+numclusters)->numcolors)++;
				(clusters+numclusters)->numpixels += (windowcolors+subsubcolorindex)->numpixels;
				
				*(usedcolors+numusedcolors) = subsubcolorindex;
				numusedcolors++;
			}
	}
	}
	//if((clusters+numclusters)->numcolors > 3)
	//printf("Cluster %d has %d pixels and %d colors\n",numclusters,(clusters+numclusters)->numpixels,(clusters+numclusters)->numcolors);
	(clusters+numclusters)->draw = 0;
	numclusters++;
}
		printf("Finished cluster detection %d clusters\n",numclusters);

/*
	for(colorindex=0;colorindex<numwindowcolors;colorindex++)
		if(!contains(usedcolors,colorindex,numusedcolors))//must have at least 1 neighbor and not have been used before
		{
			*(usedcolors+numusedcolors) = colorindex;
			numusedcolors++; 
			
			*((clusters+numclusters)->colors + ((clusters+numclusters)->numcolors)) = colorindex;
			//printf("Assigning color %d to cluster %d\n",colorindex,numclusters);
			//printcolor((windowcolors+colorindex)->color);
			(clusters+numclusters)->numcolors = 1;	
			(clusters+numclusters)->numpixels = (windowcolors+colorindex)->numpixels;
		
		for(clustercolorindex=0;clustercolorindex<(clusters+numclusters)->numcolors;clustercolorindex++)
		{
			//printf("Finding distance to color %d\n",*((clusters+numclusters)->colors+clustercolorindex));
		for(subcolorindex = colorindex+1;subcolorindex<numwindowcolors;subcolorindex++)
		{
			if(contains(usedcolors,subcolorindex,numusedcolors))
			continue;
			
			distance = colordistance((windowcolors+*((clusters+numclusters)->colors+clustercolorindex))->color,(windowcolors+subcolorindex)->color);
			
			if(distance<clusteringinitializationvariable)
			{
				printf("Assigning color %d to cluster %d because of color %d distance %g\n",subcolorindex,numclusters,distance,*((clusters+numclusters)->colors+clustercolorindex));
				*((clusters+numclusters)->colors + ((clusters+numclusters)->numcolors)) = subcolorindex;
				((clusters+numclusters)->numcolors)++;
				(clusters+numclusters)->numpixels += (windowcolors+subcolorindex)->numpixels;
				
				*(usedcolors+numusedcolors) = subcolorindex;
				numusedcolors++;
			}
			
			
			//clusterindex++;
		}
	}
	if((clusters+numclusters)->numcolors > 3)
	printf("Cluster %d has %d pixels and %d colors\n",numclusters,(clusters+numclusters)->numpixels,(clusters+numclusters)->numcolors);
		numclusters++;
	}*/
		//int clusterindex;
		//for(clusterindex=0;clusterindex<numclusters;clusterindex++)
		//{
			//printf("Cluster %d has %d pixels and %d colors\n",clusterindex,(clusters+clusterindex)->numpixels,(clusters+clusterindex)->numcolors);
		//}
		//printf("Finished cluster detection %d clusters\n",numclusters);
		free(adjacencylist);
		free(numadjacent);
}
/*
int clusteralgorithm()
{
	double distancesum;
	double distance;
	double longestdistance;
	int num;

	for(num=1,distancesum=0;num <numwindowcolors;num++)
	{
		distance = colordistance(windowcolors->color,(windowcolors+num)->color);
		longestdistance = colordistance(windowcolors->color,(windowcolors+numwindowcolors-1)->color);
		//printf("Num %d distance %g sum %g longest distance %g num colors %d %g %g\n",num,distance,distancesum,longestdistance,numwindowcolors,clusteringinitializationvariable * longestdistance/numwindowcolors,clusteringcontinuationvariable * distancesum/(num-1));
		printf("Num %d distance %g sum %g longest distance %g num colors %d %g %g\n",num,distance,distancesum,longestdistance,numwindowcolors,clusteringinitializationvariable,clusteringcontinuationvariable * distancesum/(num-1));
		if(distancesum == 0)
		{
			//if(distance > clusteringinitializationvariable * longestdistance/numwindowcolors)
			if(distance > clusteringinitializationvariable)
			{
			printf("did not start\n");
			break;
			}
			distancesum += distance;
		}
		else
		//if(distance < clusteringinitializationvariable * longestdistance/numwindowcolors || clusteringcontinuationvariable * distancesum/(num-1) > distance)
		if(distance < clusteringinitializationvariable || clusteringcontinuationvariable * distancesum/(num-1) > distance)
		{
			distancesum += distance;
		}
		else
		break;
	}
	
	printf("num %d \n",num-1);
	return num-1;
}*/
/*
GtkWidget * colorlinewidget(int clusterindex)
{
	GtkWidget *hbox = gtk_hbox_new(FALSE,0);
	GtkWidget *drawingarea = gtk_drawing_area_new();
	 gtk_widget_set_size_request (drawingarea, 30, 30);
	char buffer[1000];
	//printf("Drawing cluster %d with color %d\n",clusterindex,*((clusters+clusterindex)->colors));
	struct colordata *color = (windowcolors+*((clusters+clusterindex)->colors));
	//printf("creating widget for cluster %d\n",clusterindex);
	sprintf(buffer,"%d Pixels %d Colors?", (clusters+clusterindex)->numpixels,(clusters+clusterindex)->numcolors);
	GtkWidget *checkbox = gtk_check_button_new_with_label(buffer);
		
	gtk_toggle_button_set_active (checkbox,
                              *(drawingarray+*((clusters+clusterindex)->colors)));
	gtk_box_pack_start (GTK_BOX(hbox),drawingarea, TRUE, TRUE, 20);
	gtk_box_pack_start (GTK_BOX(hbox),checkbox, FALSE, TRUE, 20);
	
	g_signal_connect(drawingarea,"draw",G_CALLBACK(drawarea),clusterindex);
	g_signal_connect(checkbox,"toggled",G_CALLBACK(toggledraw),clusterindex);
	
	return hbox;
}*/
/*
void drawcolorwindow()
{
	int index;
	gtk_container_remove(colorwindow,colorwindowvbox);//Memory leak, need to delete vbox, lines, drawing areas, and anything else allocated previously

	colorwindowvbox = gtk_vbox_new(FALSE,0);
	GtkWidget *line;
	printf("Adding %d clusters\n",numclusters);
	for(index = 0;index< fmin(colordisplaymax,numclusters);index++)
	{
		line = colorlinewidget(index);
		gtk_box_pack_start (GTK_BOX(colorwindowvbox),line, FALSE, TRUE, 0);
		//printf("color %d\n",index);
	}
	
	gtk_container_add (GTK_CONTAINER (colorwindow), colorwindowvbox);
	gtk_widget_show_all(colorwindow);

}*/

void togglecluster(int clusterindex)
{
	
	struct cluster *c = clusters + clusterindex;
	
	c->draw = !(c->draw);
	
	int colorindex;
	for(colorindex = 0;colorindex<c->numcolors;colorindex++)
	{
		*(drawingarray+*(c->colors+colorindex)) = c->draw;
	}
}

void cleardrawarray()
{
	int colorindex;
	for(colorindex = 0;colorindex<numwindowcolors;colorindex++)
	{
		*(drawingarray+colorindex) = 0;
	}
}

void clickcolorarea(GtkWidget *widget, GdkEventMotion *event )
{
	//printf("State is %d\n",axisfree);
	//printf("Motion notify\n");
  int x, y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &x, &y, &state);
  else
    {
      x = event->x;
      y = event->y;
	}
	//axisfree = 0;
	//printf("Clicked %d x %d\n",x,y);
	int clusterindex;
	int left,top,right,bottom;
	for(clusterindex=0;clusterindex<numclusters;clusterindex++)
	{
		left = (clusters+clusterindex)->left-globalpadding/2;
		right = left + (clusters+clusterindex)->width+globalpadding;
		top = (clusters+clusterindex)->top-globalpadding/2;
		bottom = top + (clusters+clusterindex)->height+globalpadding;
		
		if(x>=left && x<right && y>= top && y<bottom)
		{
		printf("Toggling cluster %d\n",clusterindex);
		togglecluster(clusterindex);
		gtk_widget_queue_draw(grapharea);
		gtk_widget_queue_draw(colorarea);
		
		//drawgrapharea();
		//drawcolorarea();
		break;
		}
	}
}

void selectall()
{
	int index;
	for(index = 0;index<numclusters;index++)
	(clusters+index)->draw = 1;
	for(index = 0;index<numwindowcolors;index++)
	*(drawingarray + index) = 1;
	gtk_widget_queue_draw(colorarea);
	gtk_widget_queue_draw(grapharea);
}

void selectnone()
{
	int index;
	for(index = 0;index<numclusters;index++)
	(clusters+index)->draw = 0;
	for(index = 0;index<numwindowcolors;index++)
	*(drawingarray + index) = 0;
	gtk_widget_queue_draw(colorarea);
	gtk_widget_queue_draw(grapharea);
}

void selectcolorbar()
{
	selectnone();
	int i;
	for(i=0;i<numcolorbarclusters;i++)
	togglecluster(*(colorbarclusters+i));
	
	gtk_widget_queue_draw(colorarea);
	gtk_widget_queue_draw(grapharea);
}

void invertselection()
{
	int index;
	for(index = 0;index<numclusters;index++)
	(clusters+index)->draw = !((clusters+index)->draw);
	for(index = 0;index<numwindowcolors;index++)
	*(drawingarray + index) = !(*(drawingarray+index));
	gtk_widget_queue_draw(colorarea);
	gtk_widget_queue_draw(grapharea);
}

void clickgrapharea()
{
	axisfree = 0;
	colorbarclustering();
}

static gboolean
motion_notify_event( GtkWidget *widget, GdkEventMotion *event )
{
	//printf("State is %d\n",axisfree);
	//printf("Motion notify\n");
  int x, y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &x, &y, &state);
  else
    {
      x = event->x;
      y = event->y;
      state = event->state;
    }
    if(axisfree == 1)
	{
    xlow = x;
    ylow = y;
	}
	if(axisfree == 2)
	{
	xhigh = x;
	yhigh = y;
	}
	if(axisfree == 3)
	{
	xlowColorBar = x;
	ylowColorBar = y;
	}
	if(axisfree == 4)
	{
	xhighColorBar = x;
	yhighColorBar = y;
	}
	//draw_background(GraphWindow, "draw", G_CALLBACK(draw_background), NULL);
	gtk_widget_queue_draw (grapharea);
	//drawgrapharea();
  return FALSE;
}

gboolean drawgrapharea()
{
	
	GtkAllocation *allocation = (GtkAllocation *)malloc(sizeof(GtkAllocation));		//Create the drawable pixmap
	gtk_widget_get_allocation(grapharea, allocation);
	guint width = allocation->width;
	guint height = allocation->height;
	//printf("Drawing Graph Area WIth size %d x %d\n",width,height);
	
	cairo_t *cr;
	
	graphsurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width, height);
	cr = cairo_create (graphsurface);
	cairo_set_source_rgba (cr, 0, 0, 0,0);
	cairo_rectangle(cr,0,0,width,height);
	cairo_fill(cr);
	
	int colorindex,pixelindex;
	GdkColor color;

	for(colorindex=0;colorindex</*fmin(windowcolormax,numwindowcolors)*/numwindowcolors;colorindex++)
	{
		if(*(drawingarray+colorindex))
		{
		color = (windowcolors+colorindex)->color;
		cairo_set_source_rgba (cr, (double)color.red/65536, (double)color.green/65536, (double)color.blue/65536,1);
		for(pixelindex=0;pixelindex<(windowcolors+colorindex)->numpixels;pixelindex++)
		{
				cairo_rectangle(cr,*((windowcolors+colorindex)->x+pixelindex),*((windowcolors+colorindex)->y+pixelindex),1,1);
				//printf("Drawing pixel at %d  x %d\n",*((windowcolors+colorindex)->x+pixelindex),*((windowcolors+colorindex)->y+pixelindex));
		}
		cairo_fill(cr);
		}
	}
	
	cairo_set_source_rgba (cr, 0, 0, 0,1);

	//printf("Drawing grid %d x %d + %d x %d",xlow,ylow,xhigh,yhigh);
	cairo_move_to(cr,xlow,0);
	cairo_line_to(cr,xlow,height);
	cairo_move_to(cr,xhigh,0);
	cairo_line_to(cr,xhigh,height);
	cairo_move_to(cr,0,ylow);
	cairo_line_to(cr,width,ylow);
	cairo_move_to(cr,0,yhigh);
	cairo_line_to(cr,width,yhigh);
	cairo_stroke(cr);
	
	if(colorbar)
	{
	cairo_move_to(cr,xhighColorBar,yhighColorBar);
	cairo_line_to(cr,xlowColorBar,ylowColorBar);
	cairo_stroke(cr);
	}
	
	
	cairo_t *cr2;	
	cr2 = gdk_cairo_create (gtk_widget_get_window(grapharea));
	cairo_set_source_surface(cr2, graphsurface, 0, 0);
	cairo_paint(cr2);
	
	
	
	cairo_destroy(cr2);
	cairo_destroy(cr);
	return FALSE;
}

static gboolean draw_background(GtkWidget *widget, cairo_t *cr, gpointer data)
  {
	  if(transparent)
	cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.0);
	else
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
    cairo_paint(cr);
   // printf("Called\n");
    return FALSE;
  }
  static gboolean draw_background2(GtkWidget *widget, cairo_t *cr, gpointer data)
  {
	  //if(transparent)
	//cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.0);
	//else
    cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1);
    cairo_paint(cr);
   // printf("Called\n");
    return FALSE;
  }

void plotclusters(GdkColor color)
{
	
	//drawcolorwindow();
	
	printf("Plotting distance to color\n");
	FILE *outfile = fopen("colordistance.dat","w");
	int index;
	for(index = 0;index<numwindowcolors;index++)
	{
		fprintf(outfile,"%g %d\n",colordistance(color,(windowcolors+index)->color),(windowcolors+index)->numpixels);
		//fprintf(outfile,"%d %d\n",index,(windowcolors+index)->numpixels);
	}
	fclose(outfile);
	
	fprintf(clusterplotpipe,"plot 'colordistance.dat' w points\n");
	fflush(clusterplotpipe);
}

void printcolordistance(int passedindex)
{
	int index;
	for(index=0;index<numwindowcolors;index++)
	{
		printf("%d %d %g\n",index,(windowcolors+index)->numpixels,colordistance((windowcolors+passedindex)->color,(windowcolors+index)->color));
	}
}

void openclusterplotpipe()
{
	//if(absorptionplotpipe != NULL)
	//pclose(absorptionplotpipe);
	
	char *gnuplotscript = 
//	"set term postscript eps color enhanced \"Helvetica\" 24\n"
//	"set output \"%s/absorption.eps\"\n"
	"set logscale y\n"
	"set ylabel \"numpixels(log)\"\n"
	"set xlabel \"ColorDistance\"\n"
//	"set title \"Absorption\"\n"
//	"unset ytics\n"
//	"unset xtics\n"
	"set key noautotitles\n";
	
	clusterplotpipe = popen("gnuplot","w");
	fprintf(clusterplotpipe,gnuplotscript);
}

gboolean axis(int id)
{
	//0 neither, 1 x0,y3 , 2 x1,y2
	printf("ID is %d\n");
	if(id == 0 || id == 3)
	axisfree = 1;
	if(id == 1 || id == 2)
	axisfree = 2;
	if(id == 4)
	{
	axisfree = 3;
	colorbar = 1;
	}
	if(id == 5)
	{
	axisfree = 4;
	colorbar = 1;
	}
	return FALSE;
}

void getdata()
{
	GtkAllocation *allocation = (GtkAllocation *)malloc(sizeof(GtkAllocation));		//Create the drawable pixmap
	gtk_widget_get_allocation(grapharea, allocation);
	guint width = allocation->width;
	guint height = allocation->height;
	
	double x1 = 0,x2 = width,y1 = 0,y2 = height;
	printf("If there is not set axis x = %g -> %g y = %g -> %g\n",x1,x2,y1,y2);
	
	double *buffer;
	buffer = gtk_entry_get_text(xentrylow);
	printf("%s %d %g\n",buffer,strlen(buffer),atof(buffer));
	if(strlen(buffer) > 0) 
	x1 = atof(buffer);
	//else
	//x1 = 0;
	
	buffer = gtk_entry_get_text(xentryhigh);
	printf("%s %d %g\n",buffer,strlen(buffer),atof(buffer));
	if(strlen(buffer) > 0)
	x2 = atof(buffer);
	//else
	//x2 = width;
	
	buffer = gtk_entry_get_text(yentrylow);
	printf("%s %d %g\n",buffer,strlen(buffer),atof(buffer));
	if(strlen(buffer) > 0)
	y1 = atof(buffer);
	//else
	//y1 = 0;
	//
	buffer = gtk_entry_get_text(yentryhigh);
	printf("%s %d %g\n",buffer,strlen(buffer),atof(buffer));
	if(strlen(buffer) > 0)
	y2 = atof(buffer);
	//else
	//y2 = height;
	printf("x %g -> %g y %g -> %g\n",x1,x2,y1,y2); 
	
	numpoints = 0;
	int colorindex;
	for(colorindex = 0;colorindex<numwindowcolors;colorindex++)
	{
		if(*(drawingarray+colorindex))
		numpoints += (windowcolors+colorindex)->numpixels;
	}
	x = malloc(numpoints*sizeof(double));
	y = malloc(numpoints*sizeof(double));
	z = malloc(numpoints*sizeof(double));
	
	int pixelindex;
	int filledpoints = 0;
	int cbarcluster;
	int subcolor;
	for(colorindex = 0;colorindex<numwindowcolors;colorindex++)
	{
		if(*(drawingarray+colorindex))
		for(pixelindex = 0;pixelindex<(windowcolors+colorindex)->numpixels;pixelindex++)
		{
		*(x+filledpoints) = *((windowcolors+colorindex)->x + pixelindex);
		*(y+filledpoints) = *((windowcolors+colorindex)->y + pixelindex);
		if(colorbar)
		for(cbarcluster=0;cbarcluster<numcolorbarclusters;cbarcluster++)
		for(subcolor=0;subcolor<(clusters+*(colorbarclusters+cbarcluster))->numcolors;subcolor++)
		if(*((clusters+*(colorbarclusters+cbarcluster))->colors+subcolor) == colorindex)
		*(z+filledpoints) = *(colorbarclustervalues+cbarcluster);
		
		filledpoints++;
		}
	}
	int xl,xh,yl,yh;//Values to use in calculation so that xlow,xhigh, ylow, yhigh do not need to be changed
	
	if(xlow<0)
	xl = 0;
	else
	xl = xlow;
	if(ylow<0)
	yl = 0;
	else
	yl = ylow;
	if(xhigh > width)
	xh = width;
	else
	xh = xhigh;
	if(yhigh > height)
	yh = height;
	else
	yh = yhigh;
	
	for(pixelindex = 0;pixelindex<numpoints;pixelindex++)
	{
		//printf("Starting coords %g %g\n",x[pixelindex],y[pixelindex]);
		*(x+pixelindex) = (x2-x1)/(xh-xl) * (*(x+pixelindex)-xl)+x1;
		*(y+pixelindex) = (y2-y1)/(yh-yl) * (*(y+pixelindex)-yl)+y1;
		//printf("Ending coords %g %g\n",x[pixelindex],y[pixelindex]);
	}
}

void savetofile(char *filename)
{
	int index;
	FILE *outfile = fopen(filename,"w");
	if(colorbar)
	{
	for(index=0;index<numpoints;index++)
	fprintf(outfile,"%g %g %g\n",x[index],y[index],z[index]);
	}
	else
	{
	for(index=0;index<numpoints;index++)
	fprintf(outfile,"%g %g\n",x[index],y[index]);
	}
	fclose(outfile);
}

gboolean savetextdocument()
{
	printf("Saving the text doc\n");
	getdata();
	
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	//GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	gint res;

dialog = gtk_file_chooser_dialog_new ("Save File",
                                      GraphWindow,
                                      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);
chooser = GTK_FILE_CHOOSER (dialog);

gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

/*
if (user_edited_a_new_document)
  gtk_file_chooser_set_current_name (chooser,
                                     _("Untitled document"));
else
  gtk_file_chooser_set_filename (chooser,
                                 existing_filename);
*/
res = gtk_dialog_run (GTK_DIALOG (dialog));
if (res == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (chooser);
    savetofile (filename);
    g_free (filename);
  }

	gtk_widget_destroy (dialog);
	return FALSE;
}

gboolean copytoclipboard()
{
	getdata();
	savetofile ("Clipboard.txt");
	system("xclip -in -selection c Clipboard.txt");
	return FALSE;
}

gboolean savepicture()
{
	//getdata();
	
	printf("Saving the image\n");
	getdata();
	
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	//GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	gint res;

dialog = gtk_file_chooser_dialog_new ("Save Image to PNG",
                                      GraphWindow,
                                      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);
chooser = GTK_FILE_CHOOSER (dialog);

gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);


res = gtk_dialog_run (GTK_DIALOG (dialog));
if (res == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (chooser);
 
 
	cairo_surface_write_to_png(graphsurface,filename);
  }

	gtk_widget_destroy (dialog);
	
	
	
	return FALSE;
}

GtkWidget * EntryAnchor(int horizontal, int id)
{
	//Returns a box which has a 1st child as a gtk entry, and second child as an anchor button
	GtkWidget *box;
	if(horizontal)
	box = gtk_vbox_new(FALSE,0);
	else
	box = gtk_hbox_new(FALSE,0);

	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_width_chars (entry,3);
	gtk_entry_set_max_width_chars (entry,3);
	GtkWidget *button;
	if(horizontal)
	button = gtk_button_new_from_icon_name("object-flip-horizontal",GTK_ICON_SIZE_BUTTON);
	else
	button = gtk_button_new_from_icon_name("object-flip-vertical",GTK_ICON_SIZE_BUTTON);

	g_signal_connect_swapped(button, "clicked", G_CALLBACK(axis), id);

	switch(id)
	{
		case 0:
		xentrylow = entry;
		break;
		case 1:
		xentryhigh = entry;
		break;
		case 2:
		yentryhigh = entry;
		break;
		case 3:
		yentrylow = entry;
		break;
		case 4:
		colorbarlow = entry;
		break;
		case 5:
		colorbarhigh = entry;
		break;
	}
	if(horizontal)
	{
	gtk_box_pack_start(box,entry,FALSE,TRUE,5);
	gtk_box_pack_start(box,button,FALSE,TRUE,5);
	}
	else
	{
	gtk_box_pack_start(box,button,FALSE,TRUE,5);
	gtk_box_pack_start(box,entry,FALSE,TRUE,5);
	}
	return box;
}

int main( int argc, char **argv)
{
	gtk_init(NULL,NULL);
	openclusterplotpipe();
	
	GraphWindow =  gtk_window_new (GTK_WINDOW_TOPLEVEL); 
	gtk_window_set_keep_above (GraphWindow,1);
	//gtk_frame_set_shadow_type (GraphWindow, GTK_SHADOW_NONE);
	//gtk_widget_set_opacity(GraphWindow,opacity);
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_column_homogeneous(grid,0);
	gtk_grid_set_row_homogeneous(grid,0);

	
	
	
	grapharea = gtk_drawing_area_new();
	gtk_widget_set_size_request(grapharea,152,152);//Width must be multiple of 4
	gtk_widget_set_hexpand(grapharea,1);
	gtk_widget_set_vexpand(grapharea,1);
	gtk_widget_add_events (grapharea, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK);
	g_signal_connect (grapharea, "motion_notify_event",motion_notify_event, NULL);
	g_signal_connect (grapharea, "button_press_event",clickgrapharea, NULL);

	
	GtkWidget *xaxisoverlay = gtk_overlay_new();
		gtk_widget_set_size_request(xaxisoverlay,152,75);
	//gtk_container_add(xaxisoverlay,gtk_vbox_new(FALSE,0));
	GtkWidget *xaxis1 = EntryAnchor(1,0);
	GtkWidget *xaxis2 = EntryAnchor(1,1);
	gtk_widget_set_halign(xaxis1,GTK_ALIGN_START);
	gtk_widget_set_halign(xaxis2,GTK_ALIGN_END);
	gtk_overlay_add_overlay(xaxisoverlay,xaxis1);
	gtk_overlay_add_overlay(xaxisoverlay,xaxis2);
	g_signal_connect(xaxisoverlay, "draw", G_CALLBACK(draw_background2), NULL);

	GtkWidget *yaxisoverlay = gtk_overlay_new();
	gtk_widget_set_size_request(yaxisoverlay,100,152);
	//gtk_container_add(xaxisoverlay,gtk_vbox_new(FALSE,0));
	GtkWidget *yaxis1 = EntryAnchor(0,2);
	GtkWidget *yaxis2 = EntryAnchor(0,3);
	gtk_widget_set_valign(yaxis1,GTK_ALIGN_START);
	gtk_widget_set_valign(yaxis2,GTK_ALIGN_END);
	gtk_overlay_add_overlay(yaxisoverlay,yaxis1);
	gtk_overlay_add_overlay(yaxisoverlay,yaxis2);
	g_signal_connect(yaxisoverlay, "draw", G_CALLBACK(draw_background2), NULL);

	GtkWidget *colorbarbox = gtk_vbox_new(FALSE,0);
	//gtk_container_add(xaxisoverlay,gtk_vbox_new(FALSE,0));
	GtkWidget *colorbar1 = EntryAnchor(0,4);
	GtkWidget *colorbar2 = EntryAnchor(0,5);
	GtkLabel *colorbartext = gtk_label_new("Colorbar");
	GtkWidget *deletecolorbox = gtk_button_new_from_icon_name("delete",GTK_ICON_SIZE_BUTTON);;
	g_signal_connect_swapped(deletecolorbox, "clicked", G_CALLBACK(deletecolorbar), NULL);

	gtk_box_pack_start (GTK_BOX(colorbarbox),colorbartext, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(colorbarbox),colorbar1, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(colorbarbox),colorbar2, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(colorbarbox),deletecolorbox, FALSE, TRUE, 0);




	//GtkWidget *leftentry = gtk_entry_new();
	GtkWidget *savetext = gtk_button_new_from_icon_name("document-save",GTK_ICON_SIZE_BUTTON);;
	GtkWidget *copyclipboard = gtk_button_new_from_icon_name("edit-copy",GTK_ICON_SIZE_BUTTON);;
	GtkWidget *saveimage = gtk_button_new_from_icon_name("application-certificate",GTK_ICON_SIZE_BUTTON);;
	g_signal_connect_swapped(savetext,"clicked",G_CALLBACK(savetextdocument),NULL);
	g_signal_connect_swapped(copyclipboard,"clicked",G_CALLBACK(copytoclipboard),NULL);
	g_signal_connect_swapped(saveimage,"clicked",G_CALLBACK(savepicture),NULL);

	
	GtkWidget *savebox = gtk_vbox_new(FALSE,0);
	gtk_box_pack_start (GTK_BOX(savebox),savetext, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(savebox),copyclipboard, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(savebox),saveimage, FALSE, TRUE, 0);


	GtkWidget *scanbutton = gtk_button_new_with_label("Read Colors");
	g_signal_connect_swapped(scanbutton,"clicked",G_CALLBACK(readcolorsfromwindow),GraphWindow);
	g_signal_connect(scanbutton, "draw", G_CALLBACK(draw_background2), NULL);

	GtkWidget *transparencybutton = gtk_button_new_with_label("Toggle Opacity");
	g_signal_connect_swapped(transparencybutton,"clicked",G_CALLBACK(toggletransparency),GraphWindow);
	g_signal_connect(transparencybutton, "draw", G_CALLBACK(draw_background2), NULL);


	gtk_grid_attach(grid,savebox,2,1,1,1);
	gtk_grid_attach(grid,colorbarbox,2,2,1,1);

	g_signal_connect(savebox, "draw", G_CALLBACK(draw_background2), NULL);

	//gtk_grid_attach(grid,sensitivity,1,0,1,1);
	gtk_grid_attach(grid,scanbutton,0,0,1,1);
	gtk_grid_attach(grid,transparencybutton,1,0,1,1);

	gtk_grid_attach(grid,grapharea,1,1,1,1);
	gtk_grid_attach(grid,xaxisoverlay,1,2,1,1);
	gtk_grid_attach(grid,yaxisoverlay,0,1,1,1);
	
	//GtkWidget *fill1 = gtk_hbox_new(FALSE,0);
	GtkWidget *fill2 = gtk_hbox_new(FALSE,0);
	GtkWidget *fill3 = gtk_hbox_new(FALSE,0);
	GtkWidget *fill4 = gtk_hbox_new(FALSE,0);

	//g_signal_connect(fill1, "draw", G_CALLBACK(draw_background2), NULL);
	g_signal_connect(fill2, "draw", G_CALLBACK(draw_background2), NULL);
	
	g_signal_connect(fill3, "draw", G_CALLBACK(draw_background2), NULL);
	g_signal_connect(fill4, "draw", G_CALLBACK(draw_background2), NULL);
	//gtk_grid_attach(grid,fill1,0,0,1,1);
	gtk_grid_attach(grid,fill2,0,2,1,1);

	gtk_grid_attach(grid,fill3,2,0,1,1);
	gtk_grid_attach(grid,fill4,2,2,1,1);

	

	gtk_container_add (GTK_CONTAINER (GraphWindow), grid);
	//gtk_widget_show_all(xaxisoverlay);

	
	g_signal_connect_swapped(grapharea,"draw",G_CALLBACK(drawgrapharea),NULL);

	gtk_widget_set_app_paintable(GraphWindow, TRUE);
    if(gtk_widget_is_composited(GraphWindow))
      {
        GdkScreen *screen = gtk_widget_get_screen(GraphWindow);
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
        gtk_widget_set_visual(GraphWindow, visual);
      }
	  else g_print("Can't set window transparency.\n");
	
	g_signal_connect(GraphWindow, "draw", G_CALLBACK(draw_background), NULL);
	gtk_widget_show_all(GraphWindow);

	
	GtkWidget *UtilityWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL); 
	gtk_window_set_keep_above (UtilityWindow,1);
	
	GtkWidget *UtilityBox = gtk_vbox_new(FALSE,0);
	//gtk_box_pack_start (GTK_BOX(UtilityBox),scanbutton, FALSE, TRUE, 20);
    //gtk_box_pack_start (GTK_BOX(UtilityBox),transparencybutton, FALSE, TRUE, 20);                                  

	
	gtk_container_add (GTK_CONTAINER (UtilityWindow), UtilityBox);
	//gtk_widget_show_all(UtilityWindow);


	colorwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL); 
	colorarea = gtk_drawing_area_new();
	gtk_widget_set_size_request(colorarea,152,152);
	g_signal_connect(colorarea, "draw", G_CALLBACK(drawcolorarea), NULL);
	gtk_widget_add_events (colorarea,  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_MOTION_MASK);
	//g_signal_connect (grapharea, "motion_notify_event",motion_notify_event, NULL);
	g_signal_connect (colorarea, "button_press_event",clickcolorarea, NULL);
	//g_signal_connect (colorarea, "motion_notify_event",clickcolorarea, NULL);

	
	
	sensitivity = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0025, 0.1, 0.0001);
	gtk_range_set_value(sensitivity,0.03);
	g_signal_connect(sensitivity, "value-changed", G_CALLBACK(clustercallback), NULL);
	g_signal_connect(sensitivity, "value-changed", G_CALLBACK(colorbarclustering), NULL);

	//g_signal_connect(sensitivity, "value-changed", G_CALLBACK(drawcolorarea), NULL);

	GtkWidget *allbutton =  gtk_button_new_with_label("Select All");
	g_signal_connect(allbutton, "clicked", G_CALLBACK(selectall), NULL);

	GtkWidget *nonebutton =  gtk_button_new_with_label("Select None");
	g_signal_connect(nonebutton, "clicked", G_CALLBACK(selectnone), NULL);

	GtkWidget *invertbutton =  gtk_button_new_with_label("Invert Selection");
	g_signal_connect(invertbutton, "clicked", G_CALLBACK(invertselection), NULL);

	GtkWidget *colorbarselectionbutton =  gtk_button_new_with_label("Select Colorbar");
	g_signal_connect(colorbarselectionbutton, "clicked", G_CALLBACK(selectcolorbar), NULL);


	GtkWidget *colorwindowtools = gtk_hbox_new(FALSE,0);
	gtk_box_pack_start (GTK_BOX(colorwindowtools),allbutton, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(colorwindowtools),nonebutton, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(colorwindowtools),invertbutton, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(colorwindowtools),colorbarselectionbutton, FALSE, TRUE, 0);


	colorwindowvbox = gtk_vbox_new(FALSE,0);
	gtk_box_pack_start (GTK_BOX(colorwindowvbox),sensitivity, FALSE, TRUE, 20);
	gtk_box_pack_start (GTK_BOX(colorwindowvbox),colorwindowtools, FALSE, TRUE, 20);
	gtk_box_pack_start (GTK_BOX(colorwindowvbox),colorarea, TRUE, TRUE, 20);
	gtk_container_add (GTK_CONTAINER (colorwindow), colorwindowvbox);
	gtk_window_set_keep_above (colorwindow,1);


	drawingarray = malloc(maxwindowcolors*sizeof(int));
		
	gtk_main();
}

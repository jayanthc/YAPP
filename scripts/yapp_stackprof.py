#!/usr/bin/python

# yapp_stackprof.py
# Calibrate folded sub-band profiles and create a 2D plot of
#   frequency versus phase.

import sys
import getopt
import math
import numpy
import matplotlib.pyplot as plotter

# function definitions
def PrintUsage(ProgName):
    "Prints usage information."
    print "Usage: " + ProgName + " [options] <data-files>"
    print "    -h  --help                 Display this usage information"
    print "    -T  --tsys <tsys>          System temperature in K"
    print "    -G  --gain <gain>          Gain in K/Jy"
    print "    -p  --npol <npol>          Number of polarisations"
    print "    -n  --onstart <phase>      Start phase of pulse"
    print "    -f  --onstop <phase>       End phase of pulse"
    print "    -b  --basefit              Do polynomial fit baseline subtraction"
    print "    -l  --line                 1D stacked plots instead of 2D image"
    return

# defaults
doPolyfit = False
showLinePlot = False

# get the command line arguments
ProgName = sys.argv[0]
OptsShort = "hT:G:p:n:f:bl"
OptsLong = ["help", "tsys=", "gain=", "npol=", "onstart=", "onstop=",         \
            "basefit", "line"]

# get the arguments using the getopt module
try:
    (Opts, Args) = getopt.getopt(sys.argv[1:], OptsShort, OptsLong)
except getopt.GetoptError, ErrMsg:
    # print usage information and exit
    sys.stderr.write("ERROR: " + str(ErrMsg) + "!\n")
    PrintUsage(ProgName)
    sys.exit(1)

optind = 1
# parse the arguments
for o, a in Opts:
    if o in ("-h", "--help"):
        PrintUsage(ProgName)
        sys.exit()
    elif o in ("-T", "--tsys"):
        Tsys = float(a)
        optind = optind + 2
    elif o in ("-G", "--gain"):
        G = float(a)
        optind = optind + 2
    elif o in ("-p", "--npol"):
        NPol = int(a)
        optind = optind + 2
    elif o in ("-n", "--onstart"):
        on = float(a)
        optind = optind + 2
    elif o in ("-f", "--onstop"):
        off = float(a)
        optind = optind + 2
    elif o in ("-b", "--basefit"):
        doPolyfit = True
        optind = optind + 1
    elif o in ("-l", "--line"):
        showLinePlot = True
        optind = optind + 1
    else:
        PrintUsage(ProgName)
        sys.exit(1)

# user input validation
if (1 == len(sys.argv)):
    ErrMsg = "No input given"
    sys.stderr.write("ERROR: " + str(ErrMsg) + "!\n")
    # print usage information and exit
    PrintUsage(ProgName)
    sys.exit(1)

NBins = len(open(sys.argv[optind]).readlines()) - 4
x = numpy.array([float(i) / NBins for i in range(NBins)])

onBin = int(on * NBins)
offBin = int(off * NBins)

NBands = len(sys.argv) - optind

# read the centre frequencies
Bands = []
for fileProf in sys.argv[optind:]:
    Bands.append([float((open(fileProf).readline())[37:-5]), fileProf])
Bands.sort()

f = numpy.zeros(NBands)
for i in range(NBands):
    f[i] = Bands[i][0]

# read the bandwidth and duration of observation from the first file
hdr = open(sys.argv[optind])
# skip centre frequency
hdr.readline()
# read the original channel bandwidth in MHz
ChanBW = float((hdr.readline())[37:-5])
# read the bandwidth in MHz and convert to Hz
BW = float((hdr.readline())[37:-5]) * 1e6
# read duration in seconds
tObs = float((hdr.readline())[37:-3])
hdr.close()

profImg = numpy.zeros(NBands * NBins)
profImg.shape = (NBands, NBins)

for i in range(NBands):
    # read raw profile
    profImg[i] = numpy.loadtxt(Bands[i][1], dtype=numpy.float32,              \
                               comments="#", delimiter="\n")

    # extract the off-pulse regions
    baseline = profImg[i].copy()
    baseline[onBin:offBin] = numpy.median(profImg[i])

    if doPolyfit:
        # flatten the baseline using a 4th-degree polynomial fit 
        fit = numpy.polyfit(x, baseline, 4)
        y = fit[0] * x**4 + fit[1] * x**3 + fit[2] * x**2 + fit[3] * x + fit[4]
    else:
        y = numpy.median(baseline)

    # calculate the mean and RMS of the off-pulse region
    baseline = baseline - y
    offMean = numpy.mean(baseline)
    offRMS = numpy.std(baseline)

    # compute the calibration factor using eq. (7.12), Lorimer & Kramer
    C = Tsys / (offRMS * G * math.sqrt(NPol * (tObs / NBins) * BW))

    # calibrate the profile
    profImg[i] = (profImg[i] - y) * C

# matplotlib.pyplot.imshow() does not align the rows correctly with respect to
# the centre frequency of the channels, so compute the lowest frequency per
# channel, to pass to imshow()
for i in range(NBands):
    f[i] = f[i] - ((BW * 1e-6) / 2) + (ChanBW / 2)      # BW is in MHz

if showLinePlot:
    offset = numpy.amax(profImg) / 10
    if NBands < 10:
        numTicks = NBands
    else:
        numTicks = 10   # don't want more than 10 ticks on the y-axis
    step = int(math.ceil(float(NBands) / numTicks))
    yticks = numpy.array([i * offset for i in range(0, NBands, step)])
    ylabels = f[::step]
    for i in range(NBands):
        profImg[i] = profImg[i] + (i * offset)
        plotter.plot(x, profImg[i])
        plotter.yticks(yticks, map(lambda val: "%.1f" % val, f))
else:
    img = plotter.imshow(profImg, origin="lower", aspect="auto", \
                     interpolation="nearest", cmap="jet")
    img.set_extent([min(x), max(x), min(f), max(f) + (BW * 1e-6)])
    cbar = plotter.colorbar(img, orientation="vertical")
    cbar.set_label("Flux Density (Jy)")

plotter.xlabel("Phase")
plotter.ylabel("Frequency (MHz)")
plotter.show()


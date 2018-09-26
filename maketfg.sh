#!/bin/bash

set -e
set -o pipefail

###	DECLARING VARIABLES	###

# Directoy of previous source code, the last stable known, no built
LASTSRC=kernel-tfg-$1-nobuilt

# The directory of the source code with all updated files 
SRC="/home/rodrigo/mpss-3.8.3/src/linux-2.6.38+mpss3.8.3-modRo/"

# The new file to be updated
NEWFILE=$(sed -n $2p ./modfiles.txt)

# Root of this file
NEWFILEROOT="linux-2.6.38+mpss3.8.3-modRo/"$NEWFILE

# Number of the version which will be created
NEWVERSION=$3

# Directory of the new source code
NEWKERNELSRC=kernel-tfg-$NEWVERSION

###	END OF DECLARING VARIABLES	###

echo "Hello. Let's update the current kernel [kernel-tfg-$1] adding the next file: $NEWFILE."
echo "We finally will get the [kernel-tfg-$NEWVERSION] source code."

# 1 Create the new modified source code folder

echo "#1 Creating the new kernel source code..."
cp -r $LASTSRC ./$NEWKERNELSRC
if [ $? -eq 0 ]
then
	echo $NEWKERNELSRC ": Done!" 
fi

cp -r $NEWKERNELSRC ./$NEWKERNELSRC-nobuilt
if [ $? -eq 0 ]
then
	echo $NEWKERNELSRC-nobuilt ": Done! (In this folder we'll keep the code without compile it)"
fi

# 2 Copy a well-known config for the new kernel version

echo "#2 Adding the .config file from a well-knwon configuration" 
cp /opt/mpss/3.8.3/sysroots/k1om-mpss-linux/boot/config-2.6.38.8+mpss3.8.3 $NEWKERNELSRC/.config
if [ $? -eq 0 ]
then
    	echo $NEWKERNELSRC/.config ": Done!"
fi

cp /opt/mpss/3.8.3/sysroots/k1om-mpss-linux/boot/config-2.6.38.8+mpss3.8.3 $NEWKERNELSRC-nobuilt/.config
if [ $? -eq 0 ]
then
    	echo $NEWKERNELSRC-nobuilt/.config ": Done!"
fi

# 3 Copy the file to update

echo "#3 Copying $NEWFILEROOT to  $NEWKERNELSRC/$NEWFILE..."

cp $NEWFILEROOT $NEWKERNELSRC/$NEWFILE
if [ $? -eq 0 ]
then
    	echo  $NEWKERNELSRC/$NEWFILE ": Done!"
fi

cp $NEWFILEROOT $NEWKERNELSRC-nobuilt/$NEWFILE
if [ $? -eq 0 ]
then
    	echo  $NEWKERNELSRC-nobuilt/$NEWFILE ": Done!"
fi

# 4 Modificate a file explaining modifications
echo "#4 Registering the file updated"

touch $NEWKERNELSRC/UPDATEDFILES.txt
if [ $? -eq 0 ]
then 
        echo $NEWKERNELSRC/UPDATEDFILES.txt ": Ok"
fi
echo "$NEWFILE" >> $NEWKERNELSRC/UPDATEDFILES.txt
if [ $? -eq 0 ]
then
    	echo  $NEWKERNELSRC/UPDATEDFILES.txt ": Writed!"
fi

touch $NEWKERNELSRC-nobuilt/UPDATEDFILES.txt
if [ $? -eq 0 ]
then
    	echo $NEWKERNELSRC-nobuilt/UPDATEDFILES.txt ": Ok"
fi
echo "$NEWFILE" >> $NEWKERNELSRC-nobuilt/UPDATEDFILES.txt
if [ $? -eq 0 ]
then
        echo  $NEWKERNELSRC-nobuilt/UPDATEDFILES.txt ": Writed!"
fi

# 5 Prepare the environment

echo "#5 Preparing the environment..."
. /opt/mpss/3.8.3/environment-setup-k1om-mpss-linux

if [ $? -eq 0 ]
then
    	echo  "Done!"
fi

# 6 Make the kernel

echo "#6 Making the new kernel..."
make -C $NEWKERNELSRC ARCH=k1om CROSS_COMPILE=k1om-mpss-linux- -j4
if [ $? -eq 0 ]
then
    	echo  "Done!"
fi

#End of script

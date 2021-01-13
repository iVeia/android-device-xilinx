#Make sure mkimage from the u-boot package is in the path
 ifeq (, $(shell which mkimage))
 $(error "No mkimage in your path.  Please set path to include u-boot mkimage")
 endif

SCRIPTS=$(wildcard *.sh)
IMGS=$(SCRIPTS:.sh=.img)

all: $(IMGS)

%.img: %.sh
	mkimage -T script -C none -n '$<' -d $< $@

clean:
	rm *.img

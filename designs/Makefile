
.DEFAULT: $@
		silice-make.py -s $@.si -b bare -p basic -o BUILD_$(subst :,_,$@)  $(ARGS)
		cd .. ; ./synth_bram.sh /BUILD_$(subst :,_,$@)/build ; cd -

clean:
	rm -rf BUILD_*

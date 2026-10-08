.PHONY: check clean clean-all
check: $(TARGETS)
	$(WIN) compare $^

clean-all:
	@echo Full Git reset is unsupported in the Windows source snapshot.
	@exit /b 1

%/:
	$(WIN) mkdir $(@D)

$(BUILD)/%.c.o: %.c | $$(@D)/
	$(WIN) compile --source "$<" --output "$@" --compiler "$(WINDOWS_TOOLS)/old-gcc/$(CC1VER)/cc1.exe" --cppflags="$(CPPFLAGS)" --cc1flags="$(CC1FLAGS)" --masflags="$(MASFLAGS)" --asflags="$(ASFLAGS)"

$(BUILD)/%.c.d: %.c | $$(@D)/
	$(CPP) $(CPPFLAGS) -M -MF $@ -MT $(@:.d=.o) -MG $<

$(BUILD)/%.s.o: %.s | $$(@D)/
	$(AS) $(ASFLAGS) --MD $(@:.o=.d) -o $@ $<

%.s.o: %.s | $$(@D)/
	$(AS) $(ASFLAGS) -no-pad-sections --MD $(@:.o=.d) -o $@ $<

%.img.bin %.img.sym &: %.img.png | $$(@D)/
	$(VPYTHON) -m tools.splat_ext.$(word 2,$(subst ., ,$(@F))) $< $(@D)/$(*F).img.bin

%.img.png.o: %.img.bin %.img.sym | $$(@D)/
	$(WIN) image-object $< $*.img.sym $@

$(BUILD)/assets/%.BIN.vsString.yaml: assets/%.BIN.yaml | $$(@D)/
	$(VPYTHON) -m tools.etc.vsString_dumpTable data/$*.BIN $< $@

$(BUILD)/%.vsString $(BUILD)/%.h $(BUILD)/%.vsString.bin &: $(BUILD)/%.vsString.yaml | $$(@D)/
	$(VPYTHON) -m tools.etc.vsString_yamlToData $< $(BUILD)/$*.vsString $(BUILD)/$*.h

%.vsString.yaml.o: %.vsString.bin
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

$(BUILD)/data/%: $(BUILD)/assets/%.vsString.bin | $$(@D)/
	$(WIN) copy $< $@

$(BUILD)/data/MENU/ITEMNAME.BIN: $(BUILD)/assets/MENU/ITEMNAME.BIN.vsStringFixed.bin | $$(@D)/
	$(WIN) copy $< $@

$(BINTARGETS): private LDFLAGS += --oformat=binary -e 0x0 $(addprefix -R,$(LDLIBS))
$(BINTARGETS): $(BUILD)/data/%: $(BUILD)/data/%.elf
$(BINTARGETS):
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION)
	$(FIXUP)

$(BINTARGETS:=.elf): private LDFLAGS += --unresolved-symbols=ignore-all --noinhibit-exe
$(BINTARGETS:=.elf): | $$(@D)/
$(BINTARGETS:=.elf):
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION)

$(BUILD)/config/%/link.d: $$(wildcard config/%/splat.yaml config/%/Makefile config/%/symbol_addrs.txt) | $$(@D)/
	$(VPYTHON) -m splat split --disassemble-all config/splat.config.yaml $<

ifneq ($(filter clean clean-all,$(MAKECMDGOALS)),)
else
include $(BINARIES:%=$(BUILD)/config/%/link.d)
endif

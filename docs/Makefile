ALL: site

SITE ?= auger:/var/www/html/pulsepins/
WAVEDROM ?= wavedrom
WAVEDROM_PATH ?= ${HOME}/.local/bin:${PATH}
WAVEDROM_STREAMER_SRC := $(wildcard wavedrom/streamer/*.json5)
WAVEDROM_STREAMER_SVG := $(patsubst wavedrom/streamer/%.json5,docs/img/timing/%.svg,$(WAVEDROM_STREAMER_SRC))

.PHONY: site timing-diagrams upload clean

site: mkdocs.yml main.py
	mkdocs build

timing-diagrams: $(WAVEDROM_STREAMER_SVG)

docs/img/timing/%.svg: wavedrom/streamer/%.json5
	PATH="$(WAVEDROM_PATH)" $(WAVEDROM) --input "$<" --indent 2 > "$@"
	perl -0pi -e 's/\n+\z/\n/' "$@"

upload: site
	scp -r site ${SITE}

clean:
	rm -rf site/

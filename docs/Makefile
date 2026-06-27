ALL: site

SITE ?= auger:/var/www/html/pulsepins/

site: mkdocs.yml main.py $(shell find docs)
	mkdocs build

upload: site
	scp -r site ${SITE}

clean:
	rm -rf site/

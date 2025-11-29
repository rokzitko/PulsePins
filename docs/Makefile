ALL: site

SITE ?= auger:/var/www/html/pulsepins/

site: mkdocs.yml $(shell find docs)
	mkdocs build

upload: site
	scp -r site @${SITE}

clean:
	rm -rf site/

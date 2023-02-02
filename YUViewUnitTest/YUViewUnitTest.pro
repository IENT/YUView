TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = filesource \
          parser \
          statistics \
          video

TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = statisticsFileCSV/StatisticsFileCSVTest.pro \
          statisticsFileVTMBMS/StatisticsFileVTMBMSTest.pro \
          statisticsData/StatisticsDataTest.pro

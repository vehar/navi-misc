""" wordoftheday.py

The sole purpose of this class is to find the word of the day on
dictionary.com and store it for use in Hangman.
"""

import re
from urllib2 import urlopen

class wordoftheday:
  """ Find the word of the day on dictionary.com
	    """
  def get_word(self):
    """ Return the word and clue in a dictionary object for use in Hangman.
		    """
    page = urlopen('http://www.dictionary.com').read()
    word = re.search('<a href="/wordoftheday/">([^<]+)',page).group(1)
		
    page = urlopen('http://dictionary.reference.com/wordoftheday/').read()
    clue = re.search('<!--\s*WOTD="'+word+'"\s*-->\s*(.\s*[^<]+)',page).group(1)

    return dict([(word, clue)])

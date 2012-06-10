class HgRepo(object):
    """Repo object representing a mercurial repository.
    """

    def __init__(self, hgrepo):
        """Initializes a new repo
        """

        self.hgrepo = hgrepo
